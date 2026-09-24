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
// Section: modules.filesystem
// ----------------------------------------------------------------
struct FilesystemModuleConfig {
    bool                     enabled = false;
    std::vector<std::string> watch_paths;
};

// ----------------------------------------------------------------
// Section: modules
// ----------------------------------------------------------------
struct ModulesConfig {
    FilesystemModuleConfig filesystem;
};

// ----------------------------------------------------------------
// Top-level config
// ----------------------------------------------------------------
struct Config {
    LoggingConfig logging;
    ModulesConfig modules;

    // Warnings collected during load.
    std::vector<std::string> load_warnings;

    // Load the config from a file.
    static Config load(const std::string& path);
};

} // namespace axiom::core