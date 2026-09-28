// src/foundation/config.h
//
// Centralized configuration loading for the AxiomDLP Agent.

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
// ----------------------------------------------------------------
struct TransportModuleConfig {
    bool        enabled    = false;
    std::string server_url = "ws://127.0.0.1:8765";

    int  reconnect_backoff_seconds     = 5;
    int  reconnect_backoff_max_seconds = 60;
    bool verify_tls                    = true;
};

// ----------------------------------------------------------------
// Section: modules.agent_sync
// ----------------------------------------------------------------
struct AgentSyncModuleConfig {
    bool        enabled = true;

    std::string http_base_url = "http://127.0.0.1:3000";
    std::string api_token;

    int  heartbeat_interval_seconds = 30;
    bool verify_tls = true;
};

// ----------------------------------------------------------------
// Section: modules.content_scanner
//
// Reads text files, splits them into overlapping chunks, sends the
// chunks to the server, and writes the merged verdict back to the
// files table.
//
// Only files whose extension is in `text_extensions` are scanned.
// PDFs, DOCX, XLSX and other rich documents require an extraction
// pipeline that is not implemented yet.
// ----------------------------------------------------------------
struct ContentScannerModuleConfig {
    bool enabled = true;

    // Maximum number of chunks per HTTP POST.
    int batch_size = 50;

    // Maximum total JSON payload size per POST.
    std::size_t batch_max_bytes = 50 * 1024 * 1024;   // 50 MB

    // Sleep between scan cycles when there is nothing to do.
    int scan_interval_seconds = 30;

    // Chunking parameters. Overlap must be large enough to cover any
    // PII pattern that could straddle a chunk boundary. 256 bytes
    // covers every common pattern (credit cards, SSNs, IBANs, keys).
    std::size_t chunk_size_bytes    = 1024 * 1024;   // 1 MB
    std::size_t chunk_overlap_bytes = 256;

    // Largest file we will read and scan. Matches the inventory limit.
    std::size_t max_file_bytes = 100 * 1024 * 1024;   // 100 MB

    // Stop retrying a file after this many failures.
    int max_retries = 3;

    // HTTP timeout for one POST.
    int request_timeout_seconds = 300;

    // Only files with these extensions are scanned by this module.
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
    ModulesConfig modules;

    std::vector<std::string> load_warnings;

    static Config load(const std::string& path);
};

} // namespace axiom::core