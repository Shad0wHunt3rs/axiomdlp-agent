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

struct ModulesConfig {
    FilesystemWatcherModuleConfig filesystem_watcher;
    InventoryModuleConfig         inventory;
};

struct Config {
    LoggingConfig logging;
    StorageConfig storage;
    ModulesConfig modules;

    std::vector<std::string> load_warnings;

    static Config load(const std::string& path);
};

} // namespace axiom::core