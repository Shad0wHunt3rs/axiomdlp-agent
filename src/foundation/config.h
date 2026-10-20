// src/foundation/config.h
#pragma once

#include <cstddef>
#include <cstdint>
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

struct ServerConfig {
    std::string base_url = "http://127.0.0.1:3000";
    std::string api_token;
    int heartbeat_interval_seconds = 30;
    int reconnect_backoff_seconds     = 5;
    int reconnect_backoff_max_seconds = 60;

    // gRPC specific configuration
    int  grpc_port = 50051;
    bool grpc_tls  = true;
    std::string grpc_ca_path;
};

struct FilesystemWatcherModuleConfig {
    bool                     enabled = false;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      debounce_ms = 300;
};

struct InventoryModuleConfig {
    bool                     enabled = true;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      interval_seconds = 900;
    bool          compute_hashes      = true;
    std::uint64_t max_file_size_bytes = 104857600ull;
    std::vector<std::string> scannable_categories = {"document", "code", "data", "config"};
};

struct AgentSyncModuleConfig {
    bool enabled = true;
};

struct ContentScannerModuleConfig {
    bool enabled = true;
    int         batch_size            = 50;
    std::size_t batch_max_bytes       = 50 * 1024 * 1024;
    int         scan_interval_seconds = 30;
    std::size_t chunk_size_bytes    = 1024 * 1024;
    std::size_t chunk_overlap_bytes = 256;
    std::size_t max_file_bytes          = 100 * 1024 * 1024;
    int         max_retries             = 3;
    int         request_timeout_seconds = 300;
    std::vector<std::string> text_extensions = {
        "txt", "md", "log", "csv", "tsv", "json", "yaml", "yml", "toml", "xml",
        "c", "cpp", "cc", "cxx", "h", "hpp", "hxx", "py", "js", "ts", "jsx", "tsx",
        "go", "rs", "java", "kt", "scala", "cs", "swift", "rb", "php", "pl", "lua",
        "sql", "sh", "bash", "zsh", "ps1", "html", "css", "scss", "less", "conf",
        "cfg", "ini", "env", "properties"
    };
};

struct ModulesConfig {
    FilesystemWatcherModuleConfig filesystem_watcher;
    InventoryModuleConfig         inventory;
    AgentSyncModuleConfig         agent_sync;
    ContentScannerModuleConfig    content_scanner;
};

struct Config {
    LoggingConfig logging;
    StorageConfig storage;
    ServerConfig  server;
    ModulesConfig modules;
    std::vector<std::string> load_warnings;
    static Config load(const std::string& path);
};

} // namespace axiom::core