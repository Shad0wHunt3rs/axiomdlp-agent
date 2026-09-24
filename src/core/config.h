// src/core/config.h
//
// Centralized configuration loading for the AxiomDLP Agent.
//
// Reads config/agent.default.json, validates values, applies defaults,
// and returns a populated Config object.
//
// Design:
//   - Nested structs mirror the JSON structure.
//   - Every field has a safe default.
//   - Loading never throws; problems are collected as warnings.

// src/core/config.h

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
// Section: modules.filesystem
// ----------------------------------------------------------------
struct FilesystemModuleConfig {
    bool                     enabled = false;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      debounce_ms = 300;
    int                      max_file_size_mb = 100;
};

// ----------------------------------------------------------------
// Section: modules.metadata
// ----------------------------------------------------------------
struct MetadataModuleConfig {
    bool                     enabled = true;
    std::vector<std::string> watch_paths;
    std::vector<std::string> exclude_paths;
    int                      interval_seconds = 900;
};

// ----------------------------------------------------------------
// Section: modules
// ----------------------------------------------------------------
struct ModulesConfig {
    FilesystemModuleConfig filesystem;
    MetadataModuleConfig   metadata;
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