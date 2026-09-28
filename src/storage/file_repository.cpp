// src/storage/file_repository.cpp

#include "storage/file_repository.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

#include <chrono>
#include <string>

namespace axiom::core {

namespace {

// RAII wrapper for a prepared statement.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            SPDLOG_ERROR("FileStore: prepare failed: {}", sqlite3_errmsg(db));
            stmt_ = nullptr;
        }
    }
    ~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const { return stmt_; }
    bool ok() const { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

std::int64_t now_seconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string text_col(sqlite3_stmt* s, int idx) {
    const unsigned char* p = sqlite3_column_text(s, idx);
    return p ? reinterpret_cast<const char*>(p) : "";
}

// Column list for every SELECT that returns a full FileRow. Keeping
// it in one place means adding a column only requires one edit.
constexpr const char* kFileRowColumns =
    "id, dev, inode, path, basename, dirname, extension, "
    "size, mtime_ns, ctime_ns, atime_ns, mode, file_type, "
    "is_directory, is_symlink, is_hidden, is_executable, "
    "uid, gid, nlink, category, "
    "is_sensitive, sensitivity, scan_status, "
    "content_hash, last_scanned_at, scan_attempts, scan_error";

// Read a full FileRow from the current row of `s`, starting at column
// index `c`. `c` is advanced past every column read.
FileRow read_file_row(sqlite3_stmt* s, int& c) {
    FileRow r;
    (void)sqlite3_column_int64(s, c++);  // id (skipped)
    r.dev             = static_cast<std::uint64_t>(sqlite3_column_int64(s, c++));
    r.inode           = static_cast<std::uint64_t>(sqlite3_column_int64(s, c++));
    r.path            = text_col(s, c++);
    r.basename        = text_col(s, c++);
    r.dirname         = text_col(s, c++);
    r.extension       = text_col(s, c++);
    r.size            = static_cast<std::uint64_t>(sqlite3_column_int64(s, c++));
    r.mtime_ns        = sqlite3_column_int64(s, c++);
    r.ctime_ns        = sqlite3_column_int64(s, c++);
    r.atime_ns        = sqlite3_column_int64(s, c++);
    r.mode            = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.file_type       = text_col(s, c++);
    r.is_directory    = sqlite3_column_int(s, c++) != 0;
    r.is_symlink      = sqlite3_column_int(s, c++) != 0;
    r.is_hidden       = sqlite3_column_int(s, c++) != 0;
    r.is_executable   = sqlite3_column_int(s, c++) != 0;
    r.uid             = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.gid             = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.nlink           = static_cast<std::uint32_t>(sqlite3_column_int64(s, c++));
    r.category        = text_col(s, c++);
    r.is_sensitive    = sqlite3_column_int(s, c++) != 0;
    r.sensitivity     = text_col(s, c++);
    r.scan_status     = text_col(s, c++);
    r.content_hash    = text_col(s, c++);
    r.last_scanned_at = sqlite3_column_int64(s, c++);
    r.scan_attempts   = sqlite3_column_int(s, c++);
    r.scan_error      = text_col(s, c++);
    return r;
}

} // namespace

// ----------------------------------------------------------------
// Schema
// ----------------------------------------------------------------

bool FileStore::initialize_schema() {
    if (!db_.is_open()) {
        SPDLOG_ERROR("FileStore: database not open");
        return false;
    }
    return db_.exec("SELECT 1 FROM files LIMIT 0;");
}

// ----------------------------------------------------------------
// Transactions
// ----------------------------------------------------------------

bool FileStore::begin_transaction() { return db_.begin_transaction(); }
bool FileStore::commit()            { return db_.commit(); }
bool FileStore::rollback()          { return db_.rollback(); }

// ----------------------------------------------------------------
// Upsert
// ----------------------------------------------------------------

bool FileStore::upsert_file(const FileRow& row, std::uint64_t walk_id) {
    const char* sql = R"SQL(
        INSERT INTO files
            (dev, inode, path, basename, dirname, extension,
             size, mtime_ns, ctime_ns, atime_ns,
             mode, file_type,
             is_directory, is_symlink, is_hidden, is_executable,
             uid, gid, nlink, category,
             is_sensitive, sensitivity, scan_status,
             content_hash, last_scanned_at, scan_attempts, scan_error,
             first_seen, last_seen, walk_id)
        VALUES
            (?, ?, ?, ?, ?, ?,
             ?, ?, ?, ?,
             ?, ?,
             ?, ?, ?, ?,
             ?, ?, ?, ?,
             ?, ?, ?,
             ?, ?, ?, ?,
             ?, ?, ?)
        ON CONFLICT(dev, inode) DO UPDATE SET
            path           = excluded.path,
            basename       = excluded.basename,
            dirname        = excluded.dirname,
            extension      = excluded.extension,
            size           = excluded.size,
            mtime_ns       = excluded.mtime_ns,
            ctime_ns       = excluded.ctime_ns,
            atime_ns       = excluded.atime_ns,
            mode           = excluded.mode,
            file_type      = excluded.file_type,
            is_directory   = excluded.is_directory,
            is_symlink     = excluded.is_symlink,
            is_hidden      = excluded.is_hidden,
            is_executable  = excluded.is_executable,
            uid            = excluded.uid,
            gid            = excluded.gid,
            nlink          = excluded.nlink,
            category       = excluded.category,
            content_hash   = excluded.content_hash,
            scan_status    = CASE
                WHEN files.content_hash = excluded.content_hash
                    THEN files.scan_status
                ELSE excluded.scan_status
            END,
            last_seen      = excluded.last_seen,
            walk_id        = excluded.walk_id
    )SQL";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    const std::int64_t now = now_seconds();

    sqlite3_bind_int64(stmt.get(),  1, static_cast<sqlite3_int64>(row.dev));
    sqlite3_bind_int64(stmt.get(),  2, static_cast<sqlite3_int64>(row.inode));
    sqlite3_bind_text (stmt.get(),  3, row.path.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(),  4, row.basename.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(),  5, row.dirname.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(),  6, row.extension.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt.get(),  7, static_cast<sqlite3_int64>(row.size));
    sqlite3_bind_int64(stmt.get(),  8, row.mtime_ns);
    sqlite3_bind_int64(stmt.get(),  9, row.ctime_ns);
    sqlite3_bind_int64(stmt.get(), 10, row.atime_ns);

    sqlite3_bind_int64(stmt.get(), 11, static_cast<sqlite3_int64>(row.mode));
    sqlite3_bind_text (stmt.get(), 12, row.file_type.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int  (stmt.get(), 13, row.is_directory  ? 1 : 0);
    sqlite3_bind_int  (stmt.get(), 14, row.is_symlink    ? 1 : 0);
    sqlite3_bind_int  (stmt.get(), 15, row.is_hidden     ? 1 : 0);
    sqlite3_bind_int  (stmt.get(), 16, row.is_executable ? 1 : 0);

    sqlite3_bind_int64(stmt.get(), 17, static_cast<sqlite3_int64>(row.uid));
    sqlite3_bind_int64(stmt.get(), 18, static_cast<sqlite3_int64>(row.gid));
    sqlite3_bind_int64(stmt.get(), 19, static_cast<sqlite3_int64>(row.nlink));
    sqlite3_bind_text (stmt.get(), 20, row.category.c_str(),    -1, SQLITE_TRANSIENT);

    sqlite3_bind_int  (stmt.get(), 21, row.is_sensitive ? 1 : 0);
    sqlite3_bind_text (stmt.get(), 22, row.sensitivity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt.get(), 23, row.scan_status.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_text (stmt.get(), 24, row.content_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 25, row.last_scanned_at);
    sqlite3_bind_int  (stmt.get(), 26, row.scan_attempts);
    sqlite3_bind_text (stmt.get(), 27, row.scan_error.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt.get(), 28, now);
    sqlite3_bind_int64(stmt.get(), 29, now);
    sqlite3_bind_int64(stmt.get(), 30, static_cast<sqlite3_int64>(walk_id));

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Pruning
// ----------------------------------------------------------------

bool FileStore::prune_stale(std::uint64_t current_walk_id) {
    const char* sql = "DELETE FROM files WHERE walk_id < ?;";
    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(current_walk_id));
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Walk bookkeeping
// ----------------------------------------------------------------

std::uint64_t FileStore::begin_walk(std::int64_t started_at) {
    const char* sql = R"SQL(
        INSERT INTO walks
            (started_at, finished_at, files_seen, files_new,
             files_changed, files_deleted, duration_ms)
        VALUES (?, 0, 0, 0, 0, 0, 0)
    )SQL";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return 0;

    sqlite3_bind_int64(stmt.get(), 1, started_at);
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) return 0;
    return static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_.handle()));
}

bool FileStore::finish_walk(std::uint64_t walk_id,
                            std::int64_t started_at,
                            std::int64_t finished_at,
                            std::size_t files_seen,
                            std::size_t files_new,
                            std::size_t files_changed,
                            std::size_t files_deleted,
                            std::int64_t duration_ms) {
    const char* sql = R"SQL(
        UPDATE walks SET
            started_at = ?, finished_at = ?,
            files_seen = ?, files_new = ?,
            files_changed = ?, files_deleted = ?,
            duration_ms = ?
        WHERE id = ?
    )SQL";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_int64(stmt.get(), 1, started_at);
    sqlite3_bind_int64(stmt.get(), 2, finished_at);
    sqlite3_bind_int64(stmt.get(), 3, static_cast<sqlite3_int64>(files_seen));
    sqlite3_bind_int64(stmt.get(), 4, static_cast<sqlite3_int64>(files_new));
    sqlite3_bind_int64(stmt.get(), 5, static_cast<sqlite3_int64>(files_changed));
    sqlite3_bind_int64(stmt.get(), 6, static_cast<sqlite3_int64>(files_deleted));
    sqlite3_bind_int64(stmt.get(), 7, duration_ms);
    sqlite3_bind_int64(stmt.get(), 8, static_cast<sqlite3_int64>(walk_id));

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Queries
// ----------------------------------------------------------------

std::vector<FileRow> FileStore::query(const FileQuery& q) const {
    std::vector<FileRow> out;

    std::string sql = std::string("SELECT ") + kFileRowColumns + " FROM files WHERE 1=1";

    if (!q.path_prefix.empty()) sql += " AND path LIKE ?";
    if (!q.category.empty())    sql += " AND category = ?";
    if (!q.extension.empty())   sql += " AND extension = ?";
    if (!q.dirname.empty())     sql += " AND dirname = ?";
    if (q.only_files)           sql += " AND is_directory = 0";
    if (q.only_sensitive)       sql += " AND is_sensitive = 1";

    std::string order_col = "path";
    if (q.order_by == "size")     order_col = "size";
    if (q.order_by == "mtime")    order_col = "mtime_ns";
    if (q.order_by == "ctime")    order_col = "ctime_ns";
    if (q.order_by == "category") order_col = "category";
    if (q.order_by == "extension")order_col = "extension";
    if (q.order_by == "dirname")  order_col = "dirname";

    sql += " ORDER BY " + order_col + (q.descending ? " DESC" : " ASC");
    sql += " LIMIT ? OFFSET ?";

    Stmt stmt(db_.handle(), sql.c_str());
    if (!stmt.ok()) return out;

    int idx = 1;
    std::string prefix_like;
    if (!q.path_prefix.empty()) {
        prefix_like = q.path_prefix + "%";
        sqlite3_bind_text(stmt.get(), idx++, prefix_like.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!q.category.empty())  sqlite3_bind_text(stmt.get(), idx++, q.category.c_str(),  -1, SQLITE_TRANSIENT);
    if (!q.extension.empty()) sqlite3_bind_text(stmt.get(), idx++, q.extension.c_str(), -1, SQLITE_TRANSIENT);
    if (!q.dirname.empty())   sqlite3_bind_text(stmt.get(), idx++, q.dirname.c_str(),   -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt.get(), idx++, static_cast<sqlite3_int64>(q.limit));
    sqlite3_bind_int64(stmt.get(), idx,   static_cast<sqlite3_int64>(q.offset));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int c = 0;
        out.push_back(read_file_row(stmt.get(), c));
    }
    return out;
}

// ----------------------------------------------------------------
// Scanner support
// ----------------------------------------------------------------

std::vector<FileRow> FileStore::fetch_pending_for_scan(std::size_t limit,
                                                       int max_attempts) const {
    std::vector<FileRow> out;

    const std::string sql = std::string("SELECT ") + kFileRowColumns +
        " FROM files "
        "WHERE (scan_status = 'pending' OR scan_status = 'scan_error') "
        "  AND content_hash != '' "
        "  AND is_directory = 0 "
        "  AND scan_attempts < ? "
        "ORDER BY last_seen DESC "
        "LIMIT ?";

    Stmt stmt(db_.handle(), sql.c_str());
    if (!stmt.ok()) return out;

    sqlite3_bind_int  (stmt.get(), 1, max_attempts);
    sqlite3_bind_int64(stmt.get(), 2, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        int c = 0;
        out.push_back(read_file_row(stmt.get(), c));
    }
    return out;
}

std::optional<ScanVerdict> FileStore::find_scan_verdict_by_hash(
        const std::string& content_hash) const {

    if (content_hash.empty()) return std::nullopt;

    const char* sql =
        "SELECT is_sensitive, sensitivity FROM files "
        "WHERE content_hash = ? AND scan_status = 'scanned' "
        "LIMIT 1";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return std::nullopt;

    sqlite3_bind_text(stmt.get(), 1, content_hash.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    ScanVerdict v;
    v.sensitive   = sqlite3_column_int(stmt.get(), 0) != 0;
    v.sensitivity = text_col(stmt.get(), 1);
    return v;
}

bool FileStore::update_scan_result_by_path(const std::string& path,
                                           const std::string& content_hash,
                                           bool sensitive,
                                           const std::string& sensitivity) {
    const char* sql =
        "UPDATE files SET "
        "  content_hash    = ?, "
        "  is_sensitive    = ?, "
        "  sensitivity     = ?, "
        "  scan_status     = 'scanned', "
        "  last_scanned_at = ?, "
        "  scan_error      = '' "
        "WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text (stmt.get(), 1, content_hash.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (stmt.get(), 2, sensitive ? 1 : 0);
    sqlite3_bind_text (stmt.get(), 3, sensitivity.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 4, now_seconds());
    sqlite3_bind_text (stmt.get(), 5, path.c_str(),           -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::record_scan_error_by_path(const std::string& path,
                                          const std::string& error) {
    const char* sql =
        "UPDATE files SET "
        "  scan_status     = 'scan_error', "
        "  scan_attempts   = scan_attempts + 1, "
        "  scan_error      = ?, "
        "  last_scanned_at = ? "
        "WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text (stmt.get(), 1, error.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 2, now_seconds());
    sqlite3_bind_text (stmt.get(), 3, path.c_str(),  -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::mark_file_missing(const std::string& path) {
    const char* sql =
        "UPDATE files SET scan_status = 'missing' WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, path.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::mark_not_applicable(const std::string& path) {
    const char* sql =
        "UPDATE files SET scan_status = 'not_applicable' WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, path.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool FileStore::mark_skipped_too_large(const std::string& path) {
    const char* sql =
        "UPDATE files SET scan_status = 'skipped_too_large' WHERE path = ?";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, path.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Transport support
// ----------------------------------------------------------------

std::uint64_t FileStore::max_file_id() const {
    Stmt stmt(db_.handle(), "SELECT COALESCE(MAX(id), 0) FROM files;");
    if (!stmt.ok()) return 0;

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        return static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 0));
    }
    return 0;
}

std::vector<FileStore::SensitiveRow>
FileStore::query_sensitive_after(std::uint64_t after_id,
                                 std::size_t limit) const {
    std::vector<SensitiveRow> out;

    Stmt stmt(db_.handle(),
              "SELECT id, path, size, sensitivity, last_seen "
              "FROM files "
              "WHERE is_sensitive = 1 AND id > ? "
              "ORDER BY id ASC LIMIT ?;");
    if (!stmt.ok()) return out;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(after_id));
    sqlite3_bind_int64(stmt.get(), 2, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        SensitiveRow r;
        r.id          = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 0));
        r.path        = text_col(stmt.get(), 1);
        r.size        = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        r.sensitivity = text_col(stmt.get(), 3);
        r.last_seen   = sqlite3_column_int64(stmt.get(), 4);
        out.push_back(std::move(r));
    }
    return out;
}

bool FileStore::mark_random_files_sensitive(std::size_t count) {
    const char* sql =
        "UPDATE files SET "
        "  is_sensitive = 1, "
        "  sensitivity = CASE (abs(random()) % 4) "
        "    WHEN 0 THEN 'mock_credit_card' "
        "    WHEN 1 THEN 'mock_pii' "
        "    WHEN 2 THEN 'mock_credentials' "
        "    ELSE 'mock_document' "
        "  END, "
        "  scan_status = 'scanned' "
        "WHERE id IN ("
        "  SELECT id FROM files "
        "  WHERE is_directory = 0 "
        "  ORDER BY random() LIMIT ? "
        ");";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(count));
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ----------------------------------------------------------------
// Aggregations
// ----------------------------------------------------------------

FileStore::Stats FileStore::stats() const {
    Stats s;

    Stmt stmt(db_.handle(),
              "SELECT "
              "  COALESCE(SUM(CASE WHEN is_directory = 0 THEN 1 ELSE 0 END), 0), "
              "  COALESCE(SUM(CASE WHEN is_directory = 1 THEN 1 ELSE 0 END), 0), "
              "  COALESCE(SUM(CASE WHEN is_directory = 0 THEN size ELSE 0 END), 0), "
              "  COALESCE(SUM(CASE WHEN is_sensitive = 1 THEN 1 ELSE 0 END), 0) "
              "FROM files;");
    if (!stmt.ok()) return s;

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        s.total_files       = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 0));
        s.total_directories = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 1));
        s.total_bytes       = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        s.sensitive_files   = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 3));
    }
    return s;
}

std::vector<FileStore::CategoryCount>
FileStore::category_breakdown() const {
    std::vector<CategoryCount> out;

    Stmt stmt(db_.handle(),
              "SELECT category, COUNT(*), COALESCE(SUM(size), 0) "
              "FROM files WHERE is_directory = 0 "
              "GROUP BY category ORDER BY COUNT(*) DESC;");
    if (!stmt.ok()) return out;

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        CategoryCount c;
        c.category = text_col(stmt.get(), 0);
        c.count    = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 1));
        c.bytes    = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        out.push_back(std::move(c));
    }
    return out;
}

std::vector<FileStore::DirCount>
FileStore::top_directories(std::size_t limit) const {
    std::vector<DirCount> out;

    Stmt stmt(db_.handle(),
              "SELECT "
              "  substr(path, 1, instr(substr(path, 2), '/') + 1) AS top_dir, "
              "  COUNT(*) AS cnt, "
              "  COALESCE(SUM(size), 0) AS bytes "
              "FROM files WHERE is_directory = 0 "
              "GROUP BY top_dir ORDER BY cnt DESC LIMIT ?;");
    if (!stmt.ok()) return out;

    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        DirCount d;
        d.dirname = text_col(stmt.get(), 0);
        d.count   = static_cast<std::size_t>(sqlite3_column_int64(stmt.get(), 1));
        d.bytes   = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 2));
        out.push_back(std::move(d));
    }
    return out;
}

} // namespace axiom::core