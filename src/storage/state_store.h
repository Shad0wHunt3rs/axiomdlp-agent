// src/storage/state_store.h
//
// StateStore is a tiny key/value store backed by the `state` table.
// It persists small pieces of agent state that must survive restarts:
//   - "agent_id"            — returned by the server at registration
//   - "agent_registered_at" — ISO timestamp of successful enrollment
//   - anything else small and non-critical
//
// It is NOT a general-purpose data store. Use FileStore or a new
// domain-specific wrapper for anything with real structure.

#pragma once

#include <optional>
#include <string>

namespace axiom::core {

class Database;

class StateStore {
public:
    explicit StateStore(Database& db) : db_(db) {}

    // Verify the `state` table exists (created by migration v3).
    bool initialize_schema();

    // Store a value, overwriting any existing value for the key.
    bool set(const std::string& key, const std::string& value);

    // Fetch a value. Returns std::nullopt if the key is absent.
    std::optional<std::string> get(const std::string& key) const;

    // Remove a key. Returns true even if the key was absent.
    bool remove(const std::string& key);

private:
    Database& db_;
};

} // namespace axiom::core