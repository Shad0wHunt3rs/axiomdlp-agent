// src/storage/database.h
//
// Thin wrapper around SQLite. Knows nothing about files, metadata, or
// DLP. Only knows how to open a database file, run SQL, and manage
// transactions.
//
// Forward-declares `sqlite3` so callers don't need to include
// <sqlite3.h> just to hold a reference.

#pragma once

#include <string>

struct sqlite3;   // opaque handle; defined in <sqlite3.h>

namespace axiom::core {

class Database {
public:
    Database() = default;
    ~Database();

    // Non-copyable, non-movable: owns the sqlite3* handle.
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    // Open (or create) the database file. Creates parent directories
    // if missing. Enables WAL and foreign keys.
    bool open(const std::string& path);

    // Close the handle. Safe to call multiple times.
    void close();

    bool is_open() const { return db_ != nullptr; }

    // Execute a statement that returns no rows. Returns false on error.
    bool exec(const std::string& sql);

    // Transaction helpers. Nested transactions are not supported.
    bool begin_transaction();
    bool commit();
    bool rollback();

    // Raw handle, for prepared statements in higher layers.
    sqlite3* handle() const { return db_; }

    // Last error message from SQLite.
    std::string last_error() const;

private:
    sqlite3* db_ = nullptr;
};

} // namespace axiom::core