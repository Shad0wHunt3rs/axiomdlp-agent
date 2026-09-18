// src/core/config.h
//
// Centralized configuration loading for the AxiomDLP Agent.
//
// Reads config/agent.default.json (or another path), validates values,
// applies defaults for missing or invalid fields, and returns a populated
// Config object.
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
// Top-level config
// ----------------------------------------------------------------
struct Config {
    LoggingConfig logging;

    // Warnings collected during load. main() logs these after the
    // logger is initialized.
    std::vector<std::string> load_warnings;

    // Load the config from a file.
    // If the file is missing or invalid, defaults are used and
    // warnings are appended to load_warnings.
    static Config load(const std::string& path);
};

} // namespace axiom::core