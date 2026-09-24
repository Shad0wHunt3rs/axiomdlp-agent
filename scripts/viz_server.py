#!/usr/bin/env python3
"""
Minimal HTTP server that exposes data/agent.db as JSON for the
visualization page. Uses only the Python standard library — no pip.

Endpoints:
    GET /                -> the HTML page
    GET /api/stats       -> totals
    GET /api/categories  -> file count + bytes per category
    GET /api/extensions  -> top extensions by count
    GET /api/top-dirs    -> top-level directory breakdown
    GET /api/walk-info   -> migrations version, file count
    GET /api/files       -> paginated file list
        ?limit=N         (default 100)
        &offset=N        (default 0)
        &order=size|mtime|path
        &category=X      (optional filter)
        &extension=X     (optional filter)
        &search=X        (optional path substring)
"""

import json
import os
import sqlite3
import sys
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

# ------------------------------------------------------------------
# Config
# ------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.dirname(SCRIPT_DIR)
DB_PATH    = os.path.join(ROOT_DIR, "data", "agent.db")
HTML_PATH  = os.path.join(SCRIPT_DIR, "viz.html")
PORT       = int(os.environ.get("VIZ_PORT", "8080"))


# ------------------------------------------------------------------
# Database helpers
# ------------------------------------------------------------------

def db_connect():
    con = sqlite3.connect(f"file:{DB_PATH}?mode=ro", uri=True)
    con.row_factory = sqlite3.Row
    return con


def get_stats():
    with db_connect() as con:
        cur = con.cursor()
        row = cur.execute("""
            SELECT
                SUM(CASE WHEN is_directory = 0 THEN 1 ELSE 0 END) AS files,
                SUM(CASE WHEN is_directory = 1 THEN 1 ELSE 0 END) AS dirs,
                SUM(CASE WHEN is_directory = 0 THEN size ELSE 0 END) AS bytes,
                SUM(CASE WHEN is_sensitive = 1 THEN 1 ELSE 0 END) AS sensitive
            FROM files
        """).fetchone()
        version = cur.execute("PRAGMA user_version").fetchone()[0]
        return {
            "files":     row["files"]     or 0,
            "dirs":      row["dirs"]      or 0,
            "bytes":     row["bytes"]     or 0,
            "sensitive": row["sensitive"] or 0,
            "schema":    version,
        }


def get_categories():
    with db_connect() as con:
        cur = con.cursor()
        rows = cur.execute("""
            SELECT category, COUNT(*) AS count, COALESCE(SUM(size), 0) AS bytes
            FROM files
            WHERE is_directory = 0
            GROUP BY category
            ORDER BY count DESC
        """).fetchall()
        return [dict(r) for r in rows]


def get_extensions():
    with db_connect() as con:
        cur = con.cursor()
        rows = cur.execute("""
            SELECT extension, COUNT(*) AS count, COALESCE(SUM(size), 0) AS bytes
            FROM files
            WHERE is_directory = 0 AND extension != ''
            GROUP BY extension
            ORDER BY count DESC
            LIMIT 25
        """).fetchall()
        return [dict(r) for r in rows]


def get_top_dirs():
    with db_connect() as con:
        cur = con.cursor()
        rows = cur.execute("""
            SELECT
                substr(path, 1, instr(substr(path, 2), '/') + 1) AS dir,
                COUNT(*) AS count,
                COALESCE(SUM(size), 0) AS bytes
            FROM files
            WHERE is_directory = 0
            GROUP BY dir
            ORDER BY count DESC
            LIMIT 20
        """).fetchall()
        return [dict(r) for r in rows]


def get_files(params):
    limit    = min(int(params.get("limit",  ["100"])[0]), 1000)
    offset   = max(int(params.get("offset", ["0"])[0]), 0)
    order    = params.get("order", ["size"])[0]
    category = params.get("category",  [""])[0]
    extension= params.get("extension", [""])[0]
    search   = params.get("search",    [""])[0]

    order_col = "size"
    if order in ("size", "mtime", "path"):
        order_col = {"size": "size", "mtime": "mtime_ns", "path": "path"}[order]

    where = ["is_directory = 0"]
    binds = []
    if category:
        where.append("category = ?")
        binds.append(category)
    if extension:
        where.append("extension = ?")
        binds.append(extension)
    if search:
        where.append("path LIKE ?")
        binds.append(f"%{search}%")

    sql = f"""
        SELECT path, basename, extension, size, mtime_ns, category,
               is_executable, uid
        FROM files
        WHERE {' AND '.join(where)}
        ORDER BY {order_col} DESC
        LIMIT ? OFFSET ?
    """
    binds.extend([limit, offset])

    with db_connect() as con:
        cur = con.cursor()
        rows = cur.execute(sql, binds).fetchall()
        return [dict(r) for r in rows]


# ------------------------------------------------------------------
# HTTP handler
# ------------------------------------------------------------------

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass  # quiet

    def _send_json(self, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _send_html(self):
        with open(HTML_PATH, "rb") as f:
            body = f.read()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url    = urlparse(self.path)
        path   = url.path
        params = parse_qs(url.query)

        try:
            if path == "/" or path == "/index.html":
                self._send_html()
            elif path == "/api/stats":
                self._send_json(get_stats())
            elif path == "/api/categories":
                self._send_json(get_categories())
            elif path == "/api/extensions":
                self._send_json(get_extensions())
            elif path == "/api/top-dirs":
                self._send_json(get_top_dirs())
            elif path == "/api/files":
                self._send_json(get_files(params))
            else:
                self.send_error(404)
        except Exception as e:
            body = json.dumps({"error": str(e)}).encode()
            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)


# ------------------------------------------------------------------
# Main
# ------------------------------------------------------------------

def main():
    if not os.path.exists(DB_PATH):
        print(f"ERROR: database not found: {DB_PATH}", file=sys.stderr)
        print("Run the agent once to populate it.", file=sys.stderr)
        sys.exit(1)

    if not os.path.exists(HTML_PATH):
        print(f"ERROR: HTML page not found: {HTML_PATH}", file=sys.stderr)
        sys.exit(1)

    server = HTTPServer(("127.0.0.1", PORT), Handler)
    print(f"Serving {DB_PATH}")
    print(f"Open http://127.0.0.1:{PORT}/ in your browser")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()