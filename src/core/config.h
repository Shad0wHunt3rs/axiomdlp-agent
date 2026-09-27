// src/core/config.h

#pragma once

#include <string>
#include <vector>

namespace axiom::core {

struct LoggingConfig {
    std::string dir   = "logs";
    std::string file  = "agent.log";
    std::string level = "info";
};

struct StorageConfig {
    std::string db_path = "data/agent.db";
};

struct FilesystemWatcherModuleConfig {
    bool                     enabled = false;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      debounce_ms = 300;
    int                      max_file_size_mb = 100;
};

struct InventoryModuleConfig {
    bool                     enabled = true;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      interval_seconds = 900;
};

struct TransportModuleConfig {
    bool        enabled    = false;
    std::string server_url = "ws://127.0.0.1:8765";

    int reconnect_backoff_seconds     = 5;
    int reconnect_backoff_max_seconds = 60;
};

// ----------------------------------------------------------------
// agent_sync — HTTP register + heartbeat to the AxiomDLP Core server.
// ----------------------------------------------------------------
struct AgentSyncModuleConfig {
    bool        enabled = true;

    // Base URL for the HTTP API. No trailing slash.
    // E.g. "http://127.0.0.1:3000"
    std::string http_base_url = "http://127.0.0.1:3000";

    // The "axdlp_user_..." enrollment token issued at user registration.
    std::string api_token;

    // How often to POST /agent/heartbeat.
    int heartbeat_interval_seconds = 30;
};

struct ModulesConfig {
    FilesystemWatcherModuleConfig filesystem_watcher;
    InventoryModuleConfig         inventory;
    TransportModuleConfig         transport;
    AgentSyncModuleConfig         agent_sync;
};

struct Config {
    LoggingConfig logging;
    StorageConfig storage;
    ModulesConfig modules;

    std::vector<std::string> load_warnings;

    static Config load(const std::string& path);
};

} // namespace axiom::core