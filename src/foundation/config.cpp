// src/foundation/config.cpp

#include "foundation/config.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdlib>
#include <fstream>

using json = nlohmann::json;

namespace axiom::core {

namespace {

// Expand environment variables in a string. Supports both Windows-style
// %VAR% and POSIX-style ${VAR}. Undefined variables are left in place
// so misconfigurations are visible in the log rather than producing an
// empty string silently.
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
                 std::vector<std::string>& warnings,
                 const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_string()) {
        warnings.push_back(section + "." + key + " must be a string; using default");
        return;
    }
    out = expand_env(obj[key].get<std::string>());
}

void read_bool(const json& obj, const char* key, bool& out,
               std::vector<std::string>& warnings,
               const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_boolean()) {
        warnings.push_back(section + "." + key + " must be a boolean; using default");
        return;
    }
    out = obj[key].get<bool>();
}

void read_int(const json& obj, const char* key, int& out,
              std::vector<std::string>& warnings,
              const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_number_integer()) {
        warnings.push_back(section + "." + key + " must be an integer; using default");
        return;
    }
    out = obj[key].get<int>();
}

void read_uint64(const json& obj, const char* key, std::uint64_t& out,
                 std::vector<std::string>& warnings,
                 const std::string& section) {
    if (!obj.contains(key)) return;
    if (!obj[key].is_number_unsigned() && !obj[key].is_number_integer()) {
        warnings.push_back(section + "." + key +
                           " must be a non-negative integer; using default");
        return;
    }
    const auto v = obj[key].get<std::int64_t>();
    if (v < 0) {
        warnings.push_back(section + "." + key +
                           " must be non-negative; using default");
        return;
    }
    out = static_cast<std::uint64_t>(v);
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
    return level == "trace"    || level == "debug" || level == "info"  ||
           level == "warn"     || level == "error" || level == "critical" ||
           level == "off";
}

// ----------------------------------------------------------------
// Section parsers
// ----------------------------------------------------------------

void parse_logging(const json& root, Config& cfg) {
    if (!root.contains("logging")) return;
    const json& log = root["logging"];
    if (!log.is_object()) return;

    read_string(log, "dir",   cfg.logging.dir,   cfg.load_warnings, "logging");
    read_string(log, "file",  cfg.logging.file,  cfg.load_warnings, "logging");
    read_string(log, "level", cfg.logging.level, cfg.load_warnings, "logging");

    if (!is_valid_level(cfg.logging.level)) {
        cfg.load_warnings.push_back("logging.level '" + cfg.logging.level +
                                    "' is not valid; using 'info'");
        cfg.logging.level = "info";
    }
}

void parse_storage(const json& root, Config& cfg) {
    if (!root.contains("storage")) return;
    const json& st = root["storage"];
    if (!st.is_object()) return;

    read_string(st, "db_path", cfg.storage.db_path,
                cfg.load_warnings, "storage");
}

void parse_filesystem_watcher(const json& mods, Config& cfg) {
    if (!mods.contains("filesystem_watcher")) return;
    const json& fw = mods["filesystem_watcher"];
    if (!fw.is_object()) return;

    auto& m = cfg.modules.filesystem_watcher;
    const std::string section = "modules.filesystem_watcher";

    read_bool         (fw, "enabled",       m.enabled,       cfg.load_warnings, section);
    read_string_array (fw, "watch_paths",   m.watch_paths,   cfg.load_warnings, section);
    read_string_array (fw, "exclude_paths", m.exclude_paths, cfg.load_warnings, section);
    read_int          (fw, "debounce_ms",   m.debounce_ms,   cfg.load_warnings, section);

    if (m.debounce_ms < 0) {
        cfg.load_warnings.push_back(section + ".debounce_ms must be >= 0; using 300");
        m.debounce_ms = 300;
    }
}

void parse_inventory(const json& mods, Config& cfg) {
    if (!mods.contains("inventory")) return;
    const json& inv = mods["inventory"];
    if (!inv.is_object()) return;

    auto& m = cfg.modules.inventory;
    const std::string section = "modules.inventory";

    read_bool         (inv, "enabled",              m.enabled,              cfg.load_warnings, section);
    read_string_array (inv, "watch_paths",          m.watch_paths,          cfg.load_warnings, section);
    read_string_array (inv, "exclude_paths",        m.exclude_paths,        cfg.load_warnings, section);
    read_int          (inv, "interval_seconds",     m.interval_seconds,     cfg.load_warnings, section);
    read_bool         (inv, "compute_hashes",       m.compute_hashes,       cfg.load_warnings, section);
    read_uint64       (inv, "max_file_size_bytes",  m.max_file_size_bytes,  cfg.load_warnings, section);
    read_string_array (inv, "scannable_categories", m.scannable_categories, cfg.load_warnings, section);

    if (m.interval_seconds < 10) {
        cfg.load_warnings.push_back(section + ".interval_seconds must be >= 10; using 900");
        m.interval_seconds = 900;
    }
}

void parse_transport(const json& mods, Config& cfg) {
    if (!mods.contains("transport")) return;
    const json& t = mods["transport"];
    if (!t.is_object()) return;

    auto& m = cfg.modules.transport;
    const std::string section = "modules.transport";

    read_bool   (t, "enabled",                       m.enabled,                       cfg.load_warnings, section);
    read_string (t, "server_url",                    m.server_url,                    cfg.load_warnings, section);
    read_int    (t, "reconnect_backoff_seconds",     m.reconnect_backoff_seconds,     cfg.load_warnings, section);
    read_int    (t, "reconnect_backoff_max_seconds", m.reconnect_backoff_max_seconds, cfg.load_warnings, section);
    read_bool   (t, "verify_tls",                    m.verify_tls,                    cfg.load_warnings, section);

    if (m.reconnect_backoff_seconds < 1) {
        cfg.load_warnings.push_back(section + ".reconnect_backoff_seconds must be >= 1; using 5");
        m.reconnect_backoff_seconds = 5;
    }
    if (m.reconnect_backoff_max_seconds < m.reconnect_backoff_seconds) {
        cfg.load_warnings.push_back(section +
            ".reconnect_backoff_max_seconds must be >= reconnect_backoff_seconds; using 60");
        m.reconnect_backoff_max_seconds = 60;
    }
}

void parse_agent_sync(const json& mods, Config& cfg) {
    if (!mods.contains("agent_sync")) return;
    const json& a = mods["agent_sync"];
    if (!a.is_object()) return;

    auto& m = cfg.modules.agent_sync;
    const std::string section = "modules.agent_sync";

    read_bool   (a, "enabled",                    m.enabled,                    cfg.load_warnings, section);
    read_string (a, "http_base_url",              m.http_base_url,              cfg.load_warnings, section);
    read_string (a, "api_token",                  m.api_token,                  cfg.load_warnings, section);
    read_int    (a, "heartbeat_interval_seconds", m.heartbeat_interval_seconds, cfg.load_warnings, section);
    read_bool   (a, "verify_tls",                 m.verify_tls,                 cfg.load_warnings, section);

    if (m.heartbeat_interval_seconds < 5) {
        cfg.load_warnings.push_back(section + ".heartbeat_interval_seconds must be >= 5; using 30");
        m.heartbeat_interval_seconds = 30;
    }

    // Strip trailing slashes so URL concatenation doesn't produce
    // double slashes like "http://host:3000//api/v1/...".
    while (!m.http_base_url.empty() && m.http_base_url.back() == '/') {
        m.http_base_url.pop_back();
    }
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

    parse_logging(root, cfg);
    parse_storage(root, cfg);

    if (root.contains("modules")) {
        const json& mods = root["modules"];
        if (mods.is_object()) {
            parse_filesystem_watcher(mods, cfg);
            parse_inventory(mods, cfg);
            parse_transport(mods, cfg);
            parse_agent_sync(mods, cfg);
        }
    }

    return cfg;
}

} // namespace axiom::core