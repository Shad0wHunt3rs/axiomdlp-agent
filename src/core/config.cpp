// src/core/config.cpp

#include "core/config.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>

using json = nlohmann::json;

namespace axiom::core {

namespace {

std::string expand_env(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;

    while (i < s.size()) {
        if (s[i] == '%') {
            const auto end = s.find('%', i + 1);
            if (end != std::string::npos && end > i + 1) {
                const std::string var = s.substr(i + 1, end - i - 1);
                const char* val = std::getenv(var.c_str());
                if (val) out += val;
                else     out += s.substr(i, end - i + 1);
                i = end + 1;
                continue;
            }
        }

        if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
            const auto end = s.find('}', i + 2);
            if (end != std::string::npos) {
                const std::string var = s.substr(i + 2, end - i - 2);
                const char* val = std::getenv(var.c_str());
                if (val) out += val;
                else     out += s.substr(i, end - i + 1);
                i = end + 1;
                continue;
            }
        }

        out += s[i++];
    }
    return out;
}

void read_string(const json& obj, const char* key, std::string& out,
                 std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_string()) {
        warnings.push_back(section + "." + key + " must be a string; using default");
        return;
    }
    out = expand_env(obj[key].get<std::string>());
}

void read_bool(const json& obj, const char* key, bool& out,
               std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_boolean()) {
        warnings.push_back(section + "." + key + " must be a boolean; using default");
        return;
    }
    out = obj[key].get<bool>();
}

void read_int(const json& obj, const char* key, int& out,
              std::vector<std::string>& warnings, const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_number_integer()) {
        warnings.push_back(section + "." + key + " must be an integer; using default");
        return;
    }
    out = obj[key].get<int>();
}

void read_string_array(const json& obj, const char* key,
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
        values.push_back(expand_env(item.get<std::string>()));
    }
    out = std::move(values);
}

bool is_valid_level(const std::string& level) {
    return level == "trace" || level == "debug" || level == "info" ||
           level == "warn"  || level == "error" || level == "critical" ||
           level == "off";
}

} // namespace

Config Config::load(const std::string& path) {
    Config cfg;

    std::ifstream file(path);
    if (!file.is_open()) {
        cfg.load_warnings.push_back("config file not found at '" + path +
                                    "'; using built-in defaults");
        return cfg;
    }

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
        }
    }

    if (root.contains("storage")) {
        const json& st = root["storage"];
        if (st.is_object()) {
            read_string(st, "db_path", cfg.storage.db_path,
                        cfg.load_warnings, "storage");
        }
    }

    if (root.contains("modules")) {
        const json& mods = root["modules"];
        if (mods.is_object()) {
            if (mods.contains("filesystem_watcher")) {
                const json& fw = mods["filesystem_watcher"];
                if (fw.is_object()) {
                    read_bool(fw, "enabled",
                              cfg.modules.filesystem_watcher.enabled,
                              cfg.load_warnings, "modules.filesystem_watcher");
                    read_string_array(fw, "watch_paths",
                                      cfg.modules.filesystem_watcher.watch_paths,
                                      cfg.load_warnings, "modules.filesystem_watcher");
                    read_string_array(fw, "exclude_paths",
                                      cfg.modules.filesystem_watcher.exclude_paths,
                                      cfg.load_warnings, "modules.filesystem_watcher");
                    read_int(fw, "debounce_ms",
                             cfg.modules.filesystem_watcher.debounce_ms,
                             cfg.load_warnings, "modules.filesystem_watcher");
                    read_int(fw, "max_file_size_mb",
                             cfg.modules.filesystem_watcher.max_file_size_mb,
                             cfg.load_warnings, "modules.filesystem_watcher");

                    if (cfg.modules.filesystem_watcher.debounce_ms < 0) {
                        cfg.load_warnings.push_back(
                            "modules.filesystem_watcher.debounce_ms must be >= 0; using 300");
                        cfg.modules.filesystem_watcher.debounce_ms = 300;
                    }
                    if (cfg.modules.filesystem_watcher.max_file_size_mb < 0) {
                        cfg.load_warnings.push_back(
                            "modules.filesystem_watcher.max_file_size_mb must be >= 0; using 100");
                        cfg.modules.filesystem_watcher.max_file_size_mb = 100;
                    }
                }
            }

            if (mods.contains("inventory")) {
                const json& inv = mods["inventory"];
                if (inv.is_object()) {
                    read_bool(inv, "enabled",
                              cfg.modules.inventory.enabled,
                              cfg.load_warnings, "modules.inventory");
                    read_string_array(inv, "watch_paths",
                                      cfg.modules.inventory.watch_paths,
                                      cfg.load_warnings, "modules.inventory");
                    read_string_array(inv, "exclude_paths",
                                      cfg.modules.inventory.exclude_paths,
                                      cfg.load_warnings, "modules.inventory");
                    read_int(inv, "interval_seconds",
                             cfg.modules.inventory.interval_seconds,
                             cfg.load_warnings, "modules.inventory");

                    if (cfg.modules.inventory.interval_seconds < 10) {
                        cfg.load_warnings.push_back(
                            "modules.inventory.interval_seconds must be >= 10; using 900");
                        cfg.modules.inventory.interval_seconds = 900;
                    }
                }
            }
        }
    }

    return cfg;
}

} // namespace axiom::core