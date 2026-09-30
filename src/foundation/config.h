// src/foundation/config.h
//
// Centralized configuration loading for the AxiomDLP Agent.
//
// All server identity and connection information lives in a single
// top-level `server` section. Modules that talk to the Core read from
// there. TLS verification is ALWAYS enabled for https:// and wss://.

#pragma once

#include <cstddef>
#include <cstdint>
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
// Section: server
//
// Single source of truth for how the agent talks to the Core.
// Both agent_sync (HTTP) and transport (WebSocket) use these values.
// content_scanner also uses them for the scan/batch endpoint.
// ----------------------------------------------------------------
struct ServerConfig {
    // Base URL of the Core, e.g. "http://127.0.0.1:3000" or
    // "https://core.example.com". Used by every module that talks
    // to the Core. Trailing slashes are stripped.
    std::string base_url = "http://127.0.0.1:3000";

    // Bearer token issued at registration: "axdlp_user_...".
    std::string api_token;

    // Heartbeat period used by agent_sync.
    int heartbeat_interval_seconds = 30;

    // WebSocket reconnect policy used by transport.
    int reconnect_backoff_seconds     = 5;
    int reconnect_backoff_max_seconds = 60;
};

// ----------------------------------------------------------------
// Section: modules.filesystem_watcher
// ----------------------------------------------------------------
struct FilesystemWatcherModuleConfig {
    bool                     enabled = false;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      debounce_ms = 300;
};

// ----------------------------------------------------------------
// Section: modules.inventory
// ----------------------------------------------------------------
struct InventoryModuleConfig {
    bool                     enabled = true;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      interval_seconds = 900;

    bool          compute_hashes      = true;
    std::uint64_t max_file_size_bytes = 104857600ull;   // 100 MB

    std::vector<std::string> scannable_categories = {
        "document", "code", "data", "config"
    };
};

// ----------------------------------------------------------------
// Section: modules.transport (WebSocket)
//
// Only two fields remain here: whether transport is on, and an
// optional explicit URL override. URL, token, and reconnect policy
// come from the top-level `server` section.
// ----------------------------------------------------------------
struct TransportModuleConfig {
    bool        enabled = true;
    // Empty = derive from server.base_url by swapping http->ws,
    // https->wss, and appending /api/v1/agent/ws.
    std::string server_url;
};

// ----------------------------------------------------------------
// Section: modules.agent_sync (HTTP)
//
// Just a toggle. URL, token, and interval come from `server`.
// ----------------------------------------------------------------
struct AgentSyncModuleConfig {
    bool enabled = true;
};

// ----------------------------------------------------------------
// Section: modules.content_scanner
//
// URL and token come from `server`. Everything here is scanner
// behaviour.
// ----------------------------------------------------------------
struct ContentScannerModuleConfig {
    bool enabled = true;

    int         batch_size            = 50;
    std::size_t batch_max_bytes       = 50 * 1024 * 1024;   // 50 MB
    int         scan_interval_seconds = 30;

    std::size_t chunk_size_bytes    = 1024 * 1024;   // 1 MB
    std::size_t chunk_overlap_bytes = 256;

    std::size_t max_file_bytes          = 100 * 1024 * 1024;   // 100 MB
    int         max_retries             = 3;
    int         request_timeout_seconds = 300;

    std::vector<std::string> text_extensions = {
        "txt", "md", "log", "csv", "tsv",
        "json", "yaml", "yml", "toml", "xml",
        "c", "cpp", "cc", "cxx", "h", "hpp", "hxx",
        "py", "js", "ts", "jsx", "tsx",
        "go", "rs", "java", "kt", "scala", "cs", "swift",
        "rb", "php", "pl", "lua", "sql", "sh", "bash", "zsh", "ps1",
        "html", "css", "scss", "less",
        "conf", "cfg", "ini", "env", "properties"
    };
};

// ----------------------------------------------------------------
// Section: modules
// ----------------------------------------------------------------
struct ModulesConfig {
    FilesystemWatcherModuleConfig filesystem_watcher;
    InventoryModuleConfig         inventory;
    TransportModuleConfig         transport;
    AgentSyncModuleConfig         agent_sync;
    ContentScannerModuleConfig    content_scanner;
};

// ----------------------------------------------------------------
// Top-level config
// ----------------------------------------------------------------
struct Config {
    LoggingConfig logging;
    StorageConfig storage;
    ServerConfig  server;
    ModulesConfig modules;

    std::vector<std::string> load_warnings;

    static Config load(const std::string& path);
};

} // namespace axiom::core