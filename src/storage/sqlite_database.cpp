// src/storage/database.cpp

#include "storage/sqlite_database.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

#include <filesystem>

namespace axiom::core {

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    if (db_ != nullptr) {
        SPDLOG_WARN("Database::open: already open; closing first");
        close();
    }

    // Make sure parent directory exists.
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            SPDLOG_ERROR("Database: cannot create parent directory '{}': {}",
                         parent.string(), ec.message());
            return false;
        }
    }

    const int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        // Read the error message BEFORE closing the handle. sqlite3_errmsg
        // is only valid while the handle is alive.
        const std::string err = db_ ? sqlite3_errmsg(db_) : "unknown";
        sqlite3_close(db_);
        db_ = nullptr;

        SPDLOG_ERROR("Database: sqlite3_open('{}') failed: {}", path, err);
        return false;
    }

    // Pragmas that make SQLite safe for concurrent readers + one writer.
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA foreign_keys=ON;");
    exec("PRAGMA busy_timeout=5000;");

    SPDLOG_INFO("Database: opened '{}'", path);
    return true;
}

void Database::close() {
    if (db_ == nullptr) return;
    sqlite3_close(db_);
    db_ = nullptr;

    // No logging here on purpose.
    //
    // close() is called from the destructor, which may run during
    // Application::shutdown() after the logger has been torn down.
    // Any SPDLOG_* call at that point dereferences a dead spdlog
    // singleton and crashes. The caller knows when it is closing the
    // database; there is nothing useful to log from here that isn't
    // already visible from "Database: opened" and the process exit.
}

bool Database::exec(const std::string& sql) {
    if (db_ == nullptr) {
        SPDLOG_ERROR("Database::exec: not open");
        return false;
    }

    char* errmsg = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        const std::string msg = errmsg ? errmsg : "unknown";
        if (errmsg) sqlite3_free(errmsg);

        SPDLOG_ERROR("Database::exec failed: {} (SQL: {})", msg, sql);
        return false;
    }
    return true;
}

bool Database::begin_transaction() { return exec("BEGIN TRANSACTION;"); }
bool Database::commit()            { return exec("COMMIT;"); }
bool Database::rollback()          { return exec("ROLLBACK;"); }

std::string Database::last_error() const {
    if (db_ == nullptr) return "not open";
    return sqlite3_errmsg(db_);
}

} // namespace axiom::core