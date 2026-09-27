// src/storage/migrations.cpp

#include "storage/migrations.h"
#include "storage/database.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

#include <string>

namespace axiom::core {

namespace {

int read_user_version(Database& db) {
    sqlite3* h = db.handle();
    if (h == nullptr) return -1;

    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2(h, "PRAGMA user_version;", -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        SPDLOG_ERROR("Migrations: cannot prepare PRAGMA user_version: {}",
                     sqlite3_errmsg(h));
        return -1;
    }

    int version = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

bool write_user_version(Database& db, int version) {
    const std::string sql = "PRAGMA user_version = " + std::to_string(version) + ";";
    return db.exec(sql);
}

// ----------------------------------------------------------------
// Migration 2: enriched files table
// ----------------------------------------------------------------
bool apply_v2(Database& db) {
    SPDLOG_INFO("Migrations: applying v2 (enriched files table)");

    const char* sql = R"SQL(
        DROP TABLE IF EXISTS files;
        DROP TABLE IF EXISTS walks;

        CREATE TABLE files (
            id             INTEGER PRIMARY KEY,
            dev            INTEGER NOT NULL,
            inode          INTEGER NOT NULL,
            path           TEXT    NOT NULL,
            basename       TEXT    NOT NULL DEFAULT '',
            dirname        TEXT    NOT NULL DEFAULT '',
            extension      TEXT    NOT NULL DEFAULT '',
            size           INTEGER NOT NULL DEFAULT 0,
            mtime_ns       INTEGER NOT NULL DEFAULT 0,
            ctime_ns       INTEGER NOT NULL DEFAULT 0,
            atime_ns       INTEGER NOT NULL DEFAULT 0,
            mode           INTEGER NOT NULL DEFAULT 0,
            file_type      TEXT    NOT NULL DEFAULT 'file',
            is_directory   INTEGER NOT NULL DEFAULT 0,
            is_symlink     INTEGER NOT NULL DEFAULT 0,
            is_hidden      INTEGER NOT NULL DEFAULT 0,
            is_executable  INTEGER NOT NULL DEFAULT 0,
            uid            INTEGER NOT NULL DEFAULT 0,
            gid            INTEGER NOT NULL DEFAULT 0,
            nlink          INTEGER NOT NULL DEFAULT 0,
            category       TEXT    NOT NULL DEFAULT 'other',

            is_sensitive   INTEGER NOT NULL DEFAULT 0,
            sensitivity    TEXT    NOT NULL DEFAULT '',
            scan_status    TEXT    NOT NULL DEFAULT 'pending',

            first_seen     INTEGER NOT NULL,
            last_seen      INTEGER NOT NULL,
            walk_id        INTEGER NOT NULL DEFAULT 0,
            UNIQUE(dev, inode)
        );

        CREATE INDEX IF NOT EXISTS idx_files_path       ON files(path);
        CREATE INDEX IF NOT EXISTS idx_files_dirname    ON files(dirname);
        CREATE INDEX IF NOT EXISTS idx_files_extension  ON files(extension);
        CREATE INDEX IF NOT EXISTS idx_files_category   ON files(category);
        CREATE INDEX IF NOT EXISTS idx_files_mtime      ON files(mtime_ns DESC);
        CREATE INDEX IF NOT EXISTS idx_files_size       ON files(size DESC);
        CREATE INDEX IF NOT EXISTS idx_files_uid        ON files(uid);
        CREATE INDEX IF NOT EXISTS idx_files_walk       ON files(walk_id);
        CREATE INDEX IF NOT EXISTS idx_files_sensitive  ON files(is_sensitive)
            WHERE is_sensitive = 1;

        CREATE TABLE IF NOT EXISTS walks (
            id            INTEGER PRIMARY KEY,
            started_at    INTEGER NOT NULL,
            finished_at   INTEGER NOT NULL,
            files_seen    INTEGER NOT NULL,
            files_new     INTEGER NOT NULL,
            files_changed INTEGER NOT NULL,
            files_deleted INTEGER NOT NULL,
            duration_ms   INTEGER NOT NULL
        );
    )SQL";

    if (!db.exec("BEGIN TRANSACTION;")) return false;
    if (!db.exec(sql)) {
        db.exec("ROLLBACK;");
        return false;
    }
    if (!db.exec("COMMIT;")) return false;

    SPDLOG_INFO("Migrations: v2 applied");
    return true;
}

// ----------------------------------------------------------------
// Migration 3: key/value state table
//
// Used by StateStore to persist small pieces of agent state that
// must survive restarts: agent_id assigned by the server, the
// enrollment timestamp, the last policy version, etc.
// ----------------------------------------------------------------
bool apply_v3(Database& db) {
    SPDLOG_INFO("Migrations: applying v3 (state table)");

    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS state (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )SQL";

    if (!db.exec("BEGIN TRANSACTION;")) return false;
    if (!db.exec(sql)) {
        db.exec("ROLLBACK;");
        return false;
    }
    if (!db.exec("COMMIT;")) return false;

    SPDLOG_INFO("Migrations: v3 applied");
    return true;
}

} // namespace

int Migrations::current_version() {
    return 3;
}

bool Migrations::apply(Database& db) {
    if (!db.is_open()) {
        SPDLOG_ERROR("Migrations: database not open");
        return false;
    }

    const int target  = current_version();
    int       current = read_user_version(db);

    if (current < 0) {
        SPDLOG_ERROR("Migrations: cannot read user_version");
        return false;
    }

    SPDLOG_INFO("Migrations: schema version {} -> {}", current, target);

    if (current >= target) {
        return true;
    }

    if (current < 2) {
        if (!apply_v2(db)) {
            SPDLOG_ERROR("Migrations: v2 failed");
            return false;
        }
        if (!write_user_version(db, 2)) {
            SPDLOG_ERROR("Migrations: cannot write user_version=2");
            return false;
        }
        current = 2;
    }

    if (current < 3) {
        if (!apply_v3(db)) {
            SPDLOG_ERROR("Migrations: v3 failed");
            return false;
        }
        if (!write_user_version(db, 3)) {
            SPDLOG_ERROR("Migrations: cannot write user_version=3");
            return false;
        }
        current = 3;
    }

    return true;
}

} // namespace axiom::core