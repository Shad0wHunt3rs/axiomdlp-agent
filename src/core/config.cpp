// src/core/config.cpp

#include "core/config.h"

#include <nlohmann/json.hpp>

#include <fstream>

using json = nlohmann::json;

namespace axiom::core {

namespace {

// Apply a string field from JSON if present and valid; otherwise keep default.
void read_string(const json& obj,
                 const char* key,
                 std::string& out,
                 std::vector<std::string>& warnings,
                 const std::string& section) {
    if (!obj.contains(key)) {
        return; // keep default silently
    }
    if (!obj[key].is_string()) {
        warnings.push_back(section + "." + key + " must be a string; using default");
        return;
    }
    out = obj[key].get<std::string>();
}

// Validate log level against the accepted set.
bool is_valid_level(const std::string& level) {
    return level == "trace"    || level == "debug" || level == "info"  ||
           level == "warn"     || level == "error" || level == "critical" ||
           level == "off";
}

} // namespace

Config Config::load(const std::string& path) {
    Config cfg;

    // --- 1. Read the file ---
    std::ifstream file(path);
    if (!file.is_open()) {
        cfg.load_warnings.push_back("config file not found at '" + path +
                                    "'; using built-in defaults");
        return cfg;
    }

    // --- 2. Parse JSON ---
    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        cfg.load_warnings.push_back(std::string("failed to parse config: ") + e.what() +
                                    "; using built-in defaults");
        return cfg;
    }

    if (!root.is_object()) {
        cfg.load_warnings.push_back("config root must be a JSON object; "
                                    "using built-in defaults");
        return cfg;
    }

    // --- 3. Extract logging section ---
    if (!root.contains("logging")) {
        cfg.load_warnings.push_back("missing 'logging' section; using defaults");
        return cfg;
    }

    const json& log = root["logging"];
    if (!log.is_object()) {
        cfg.load_warnings.push_back("'logging' must be an object; using defaults");
        return cfg;
    }

    // --- 4. Extract each field ---
    read_string(log, "dir",   cfg.logging.dir,   cfg.load_warnings, "logging");
    read_string(log, "file",  cfg.logging.file,  cfg.load_warnings, "logging");
    read_string(log, "level", cfg.logging.level, cfg.load_warnings, "logging");

    // --- 5. Validate level ---
    if (!is_valid_level(cfg.logging.level)) {
        cfg.load_warnings.push_back("logging.level '" + cfg.logging.level +
                                    "' is not valid; using 'info'");
        cfg.logging.level = "info";
    }

    return cfg;
}

} // namespace axiom::core