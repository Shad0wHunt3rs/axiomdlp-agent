// src/storage/migrations.h
//
// Schema versioning for the local SQLite database. Uses PRAGMA
// user_version to track which schema version the file currently has,
// and applies migrations in order until it matches what this build
// expects.

#pragma once

namespace axiom::core {

class Database;

class Migrations {
public:
    // Bring the database up to the current schema version.
    // Idempotent: safe to call on every startup.
    // Returns true on success, false on error.
    static bool apply(Database& db);

    // The schema version this build expects.
    static int current_version();
};

} // namespace axiom::core