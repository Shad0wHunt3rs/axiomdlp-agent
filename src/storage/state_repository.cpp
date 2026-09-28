// src/storage/state_store.cpp

#include "storage/state_repository.h"
#include "storage/sqlite_database.h"

#include <spdlog/spdlog.h>

#include <sqlite3.h>

namespace axiom::core {

namespace {

// RAII wrapper for a prepared statement, same pattern as file_store.cpp.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            SPDLOG_ERROR("StateStore: prepare failed: {}", sqlite3_errmsg(db));
            stmt_ = nullptr;
        }
    }
    ~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

    Stmt(const Stmt&)            = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const { return stmt_; }
    bool          ok()  const { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

} // namespace

// ----------------------------------------------------------------
// Schema check
// ----------------------------------------------------------------

bool StateStore::initialize_schema() {
    if (!db_.is_open()) {
        SPDLOG_ERROR("StateStore: database not open");
        return false;
    }
    return db_.exec("SELECT 1 FROM state LIMIT 0;");
}

// ----------------------------------------------------------------
// set / get / remove
// ----------------------------------------------------------------

bool StateStore::set(const std::string& key, const std::string& value) {
    const char* sql =
        "INSERT INTO state (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, key.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, value.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

std::optional<std::string> StateStore::get(const std::string& key) const {
    const char* sql = "SELECT value FROM state WHERE key = ?;";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return std::nullopt;

    sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    const unsigned char* p = sqlite3_column_text(stmt.get(), 0);
    if (p == nullptr) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(p));
}

bool StateStore::remove(const std::string& key) {
    const char* sql = "DELETE FROM state WHERE key = ?;";

    Stmt stmt(db_.handle(), sql);
    if (!stmt.ok()) return false;

    sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);

    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

} // namespace axiom::core