// src/core/config.cpp

#include "core/config.h"

#include <nlohmann/json.hpp>

#include <fstream>

using json = nlohmann::json;

namespace axiom::core {

namespace {

// Apply a string field from JSON if present and valid.
void read_string(const json& obj,
                 const char* key,
                 std::string& out,
                 std::vector<std::string>& warnings,
                 const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_string()) {
        warnings.push_back(section + "." + key + " must be a string; using default");
        return;
    }
    out = obj[key].get<std::string>();
}

// Apply a boolean field from JSON if present and valid.
void read_bool(const json& obj,
               const char* key,
               bool& out,
               std::vector<std::string>& warnings,
               const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_boolean()) {
        warnings.push_back(section + "." + key + " must be a boolean; using default");
        return;
    }
    out = obj[key].get<bool>();
}

// Apply an integer field from JSON if present and valid.
void read_int(const json& obj,
              const char* key,
              int& out,
              std::vector<std::string>& warnings,
              const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_number_integer()) {
        warnings.push_back(section + "." + key + " must be an integer; using default");
        return;
    }
    out = obj[key].get<int>();
}

// Apply an array-of-strings field from JSON if present and valid.
void read_string_array(const json& obj,
                       const char* key,
                       std::vector<std::string>& out,
                       std::vector<std::string>& warnings,
                       const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_array()) {
        warnings.push_back(section + "." + key + " must be an array; using default");
        return;
    }
    std::vector<std::string> values;
    for (const auto& item : obj[key]) {
        if (!item.is_string()) {
            warnings.push_back(section + "." + key +
                               " must contain only strings; using default");
            return;
        }
        values.push_back(item.get<std::string>());
    }
    out = std::move(values);
}

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

    // --- 3. logging section ---
    if (root.contains("logging")) {
        const json& log = root["logging"];
        if (log.is_object()) {
            read_string(log, "dir",   cfg.logging.dir,   cfg.load_warnings, "logging");
            read_string(log, "file",  cfg.logging.file,  cfg.load_warnings, "logging");
            read_string(log, "level", cfg.logging.level, cfg.load_warnings, "logging");

            if (!is_valid_level(cfg.logging.level)) {
                cfg.load_warnings.push_back("logging.level '" + cfg.logging.level +
                                            "' is not valid; using 'info'");
                cfg.logging.level = "info";
            }
        } else {
            cfg.load_warnings.push_back("'logging' must be an object; using defaults");
        }
    }

    // --- 4. modules section ---
    if (root.contains("modules")) {
        const json& mods = root["modules"];
        if (mods.is_object()) {
            // modules.filesystem
            if (mods.contains("filesystem")) {
                const json& fs = mods["filesystem"];
                if (fs.is_object()) {
                    read_bool(fs, "enabled",
                              cfg.modules.filesystem.enabled,
                              cfg.load_warnings, "modules.filesystem");
                    read_string_array(fs, "watch_paths",
                                      cfg.modules.filesystem.watch_paths,
                                      cfg.load_warnings, "modules.filesystem");
                    read_string_array(fs, "exclude_paths",
                                      cfg.modules.filesystem.exclude_paths,
                                      cfg.load_warnings, "modules.filesystem");
                    read_int(fs, "debounce_ms",
                             cfg.modules.filesystem.debounce_ms,
                             cfg.load_warnings, "modules.filesystem");
                    read_int(fs, "max_file_size_mb",
                             cfg.modules.filesystem.max_file_size_mb,
                             cfg.load_warnings, "modules.filesystem");

                    // Sanity clamps: a negative debounce would fire events
                    // immediately and defeat the purpose; a negative size
                    // limit is meaningless.
                    if (cfg.modules.filesystem.debounce_ms < 0) {
                        cfg.load_warnings.push_back(
                            "modules.filesystem.debounce_ms must be >= 0; using 300");
                        cfg.modules.filesystem.debounce_ms = 300;
                    }
                    if (cfg.modules.filesystem.max_file_size_mb < 0) {
                        cfg.load_warnings.push_back(
                            "modules.filesystem.max_file_size_mb must be >= 0; using 100");
                        cfg.modules.filesystem.max_file_size_mb = 100;
                    }
                } else {
                    cfg.load_warnings.push_back(
                        "'modules.filesystem' must be an object; using defaults");
                }
            }
        } else {
            cfg.load_warnings.push_back("'modules' must be an object; using defaults");
        }
    }

    return cfg;
}

} // namespace axiom::core