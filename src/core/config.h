// src/core/config.h
//
// Centralized configuration loading for the AxiomDLP Agent.

#pragma once

#include <string>
#include <vector>

namespace axiom::core {

// ----------------------------------------------------------------
// Section: logging
// ----------------------------------------------------------------
struct LoggingConfig {
    std::string dir   = "logs";
    std::string file  = "agent.log";
    std::string level = "info";
};

// ----------------------------------------------------------------
// Section: storage
// ----------------------------------------------------------------
struct StorageConfig {
    std::string db_path = "data/agent.db";
};

// ----------------------------------------------------------------
// Section: modules.filesystem_watcher
// ----------------------------------------------------------------
struct FilesystemWatcherModuleConfig {
    bool                     enabled = false;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      debounce_ms = 300;
    int                      max_file_size_mb = 100;
};

// ----------------------------------------------------------------
// Section: modules.inventory
// ----------------------------------------------------------------
struct InventoryModuleConfig {
    bool                     enabled = true;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      interval_seconds = 900;
};

// ----------------------------------------------------------------
// Section: modules.transport (WebSocket)
// ----------------------------------------------------------------
struct TransportModuleConfig {
    bool        enabled    = false;
    std::string server_url = "ws://127.0.0.1:8765";

    int  reconnect_backoff_seconds     = 5;
    int  reconnect_backoff_max_seconds = 60;

    // Verify the TLS certificate on wss:// connections. Must stay
    // true in production. Set to false only for a dev server with a
    // self-signed certificate, and expect a loud log warning.
    bool verify_tls = true;
};

// ----------------------------------------------------------------
// Section: modules.agent_sync (HTTP register + heartbeat)
// ----------------------------------------------------------------
struct AgentSyncModuleConfig {
    bool        enabled = true;

    std::string http_base_url = "http://127.0.0.1:3000";
    std::string api_token;

    int  heartbeat_interval_seconds = 30;
    bool verify_tls = true;
};

// ----------------------------------------------------------------
// Section: modules
// ----------------------------------------------------------------
struct ModulesConfig {
    FilesystemWatcherModuleConfig filesystem_watcher;
    InventoryModuleConfig         inventory;
    TransportModuleConfig         transport;
    AgentSyncModuleConfig         agent_sync;
};

// ----------------------------------------------------------------
// Top-level config
// ----------------------------------------------------------------
struct Config {
    LoggingConfig logging;
    StorageConfig storage;
    ModulesConfig modules;

    std::vector<std::string> load_warnings;

    static Config load(const std::string& path);
};

} // namespace axiom::core