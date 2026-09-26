// src/storage/migrations.h

#pragma once

namespace axiom::core {

class Database;

class Migrations {
public:
    static bool apply(Database& db);
    static int current_version();
};

} // namespace axiom::core