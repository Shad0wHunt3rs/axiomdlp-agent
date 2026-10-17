// src/foundation/config.cpp

#include "foundation/config.h"

#include <nlohmann/json.hpp>

#include <cstdint>
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

void warn_if_present(const json& obj, const char* key,
                     std::vector<std::string>& warnings,
                     const std::string& section,
                     const std::string& reason) {
    if (obj.contains(key)) {
        warnings.push_back(section + "." + std::string(key) +
                           " is ignored: " + reason);
    }
}

bool is_valid_level(const std::string& level) {
    return level == "trace"    || level == "debug" || level == "info"  ||
           level == "warn"     || level == "error" || level == "critical" ||
           level == "off";
}

std::string strip_trailing_slashes(std::string s) {
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
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

// Parse the top-level `server` section. If it is missing, look for
// legacy fields under modules.agent_sync so existing configs keep
// working.
void parse_server(const json& root, Config& cfg) {
    const std::string section = "server";

    if (root.contains("server") && root["server"].is_object()) {
        const json& s = root["server"];
        read_string(s, "base_url",                    cfg.server.base_url,                    cfg.load_warnings, section);
        read_string(s, "api_token",                   cfg.server.api_token,                   cfg.load_warnings, section);
        read_int   (s, "heartbeat_interval_seconds",  cfg.server.heartbeat_interval_seconds,  cfg.load_warnings, section);
        read_int   (s, "reconnect_backoff_seconds",   cfg.server.reconnect_backoff_seconds,   cfg.load_warnings, section);
        read_int   (s, "reconnect_backoff_max_seconds", cfg.server.reconnect_backoff_max_seconds, cfg.load_warnings, section);

        warn_if_present(s, "verify_tls", cfg.load_warnings, section,
                        "TLS verification is always enabled for https:// and wss:// URLs");
    } else {
        // Legacy fallback: pull from modules.agent_sync.
        if (root.contains("modules") && root["modules"].is_object()) {
            const json& mods = root["modules"];
            if (mods.contains("agent_sync") && mods["agent_sync"].is_object()) {
                const json& a = mods["agent_sync"];
                cfg.load_warnings.push_back(
                    "modules.agent_sync.{http_base_url,api_token,heartbeat_interval_seconds} "
                    "are deprecated; move them to a top-level 'server' section");
                read_string(a, "http_base_url",              cfg.server.base_url,
                            cfg.load_warnings, "modules.agent_sync");
                read_string(a, "api_token",                  cfg.server.api_token,
                            cfg.load_warnings, "modules.agent_sync");
                read_int   (a, "heartbeat_interval_seconds", cfg.server.heartbeat_interval_seconds,
                            cfg.load_warnings, "modules.agent_sync");
            }
            if (mods.contains("transport") && mods["transport"].is_object()) {
                const json& t = mods["transport"];
                read_int(t, "reconnect_backoff_seconds",
                         cfg.server.reconnect_backoff_seconds,
                         cfg.load_warnings, "modules.transport");
                read_int(t, "reconnect_backoff_max_seconds",
                         cfg.server.reconnect_backoff_max_seconds,
                         cfg.load_warnings, "modules.transport");
            }
        }
    }

    // Normalize.
    cfg.server.base_url = strip_trailing_slashes(cfg.server.base_url);

    if (cfg.server.base_url.empty()) {
        cfg.load_warnings.push_back("server.base_url is empty; using default");
        cfg.server.base_url = "http://127.0.0.1:3000";
    }
    if (cfg.server.heartbeat_interval_seconds < 5) {
        cfg.load_warnings.push_back(
            "server.heartbeat_interval_seconds must be >= 5; using 30");
        cfg.server.heartbeat_interval_seconds = 30;
    }
    if (cfg.server.reconnect_backoff_seconds < 1) {
        cfg.load_warnings.push_back(
            "server.reconnect_backoff_seconds must be >= 1; using 5");
        cfg.server.reconnect_backoff_seconds = 5;
    }
    if (cfg.server.reconnect_backoff_max_seconds < cfg.server.reconnect_backoff_seconds) {
        cfg.load_warnings.push_back(
            "server.reconnect_backoff_max_seconds must be >= reconnect_backoff_seconds; using 60");
        cfg.server.reconnect_backoff_max_seconds = 60;
    }
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

    read_bool   (t, "enabled",    m.enabled,    cfg.load_warnings, section);
    read_string (t, "server_url", m.server_url, cfg.load_warnings, section);

    // Legacy keys now live in `server`. Warn so operators move them.
    warn_if_present(t, "reconnect_backoff_seconds", cfg.load_warnings, section,
                    "moved to server.reconnect_backoff_seconds");
    warn_if_present(t, "reconnect_backoff_max_seconds", cfg.load_warnings, section,
                    "moved to server.reconnect_backoff_max_seconds");
    warn_if_present(t, "verify_tls", cfg.load_warnings, section,
                    "TLS verification is always enabled for wss:// URLs");
}

void parse_agent_sync(const json& mods, Config& cfg) {
    if (!mods.contains("agent_sync")) return;
    const json& a = mods["agent_sync"];
    if (!a.is_object()) return;

    auto& m = cfg.modules.agent_sync;
    const std::string section = "modules.agent_sync";

    read_bool(a, "enabled", m.enabled, cfg.load_warnings, section);

    // Legacy keys now live in `server`. Warn so operators move them.
    warn_if_present(a, "http_base_url", cfg.load_warnings, section,
                    "moved to server.base_url");
    warn_if_present(a, "api_token", cfg.load_warnings, section,
                    "moved to server.api_token");
    warn_if_present(a, "heartbeat_interval_seconds", cfg.load_warnings, section,
                    "moved to server.heartbeat_interval_seconds");
    warn_if_present(a, "verify_tls", cfg.load_warnings, section,
                    "TLS verification is always enabled for https:// URLs");
}

void parse_content_scanner(const json& mods, Config& cfg) {
    if (!mods.contains("content_scanner")) return;
    const json& c = mods["content_scanner"];
    if (!c.is_object()) return;

    auto& m = cfg.modules.content_scanner;
    const std::string section = "modules.content_scanner";

    read_bool   (c, "enabled",                 m.enabled,                 cfg.load_warnings, section);
    read_int    (c, "batch_size",              m.batch_size,              cfg.load_warnings, section);
    read_int    (c, "scan_interval_seconds",   m.scan_interval_seconds,   cfg.load_warnings, section);
    read_int    (c, "max_retries",             m.max_retries,             cfg.load_warnings, section);
    read_int    (c, "request_timeout_seconds", m.request_timeout_seconds, cfg.load_warnings, section);

    std::uint64_t tmp = m.batch_max_bytes;
    read_uint64 (c, "batch_max_bytes", tmp, cfg.load_warnings, section);
    m.batch_max_bytes = static_cast<std::size_t>(tmp);

    tmp = m.chunk_size_bytes;
    read_uint64 (c, "chunk_size_bytes", tmp, cfg.load_warnings, section);
    m.chunk_size_bytes = static_cast<std::size_t>(tmp);

    tmp = m.chunk_overlap_bytes;
    read_uint64 (c, "chunk_overlap_bytes", tmp, cfg.load_warnings, section);
    m.chunk_overlap_bytes = static_cast<std::size_t>(tmp);

    tmp = m.max_file_bytes;
    read_uint64 (c, "max_file_bytes", tmp, cfg.load_warnings, section);
    m.max_file_bytes = static_cast<std::size_t>(tmp);

    read_string_array (c, "text_extensions", m.text_extensions, cfg.load_warnings, section);

    if (m.batch_size < 1)            m.batch_size = 50;
    if (m.max_retries < 1)           m.max_retries = 3;
    if (m.scan_interval_seconds < 5) m.scan_interval_seconds = 30;
    if (m.chunk_size_bytes < 4096)   m.chunk_size_bytes = 1024 * 1024;
    if (m.chunk_overlap_bytes == 0)  m.chunk_overlap_bytes = 256;
    if (m.chunk_overlap_bytes >= m.chunk_size_bytes) {
        cfg.load_warnings.push_back(section +
            ".chunk_overlap_bytes must be < chunk_size_bytes; using 256");
        m.chunk_overlap_bytes = 256;
    }
}

void parse_grpc_transport(const json& mods, Config& cfg) {
    if (!mods.contains("grpc_transport")) return;
    const json& g = mods["grpc_transport"];
    if (!g.is_object()) return;

    read_bool(g, "enabled", cfg.modules.grpc_transport.enabled,
              cfg.load_warnings, "modules.grpc_transport");
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

    // Must run before parse_transport / parse_agent_sync so the legacy
    // fallback can pull values from their old locations.
    parse_server(root, cfg);

    if (root.contains("modules")) {
        const json& mods = root["modules"];
        if (mods.is_object()) {
            parse_filesystem_watcher(mods, cfg);
            parse_inventory(mods, cfg);
            parse_transport(mods, cfg);
            parse_agent_sync(mods, cfg);
            parse_content_scanner(mods, cfg);
            parse_grpc_transport(mods, cfg);
        }
    }

#ifndef AXIOM_HAVE_GRPC
    if (cfg.modules.grpc_transport.enabled) {
        cfg.load_warnings.push_back(
            "modules.grpc_transport.enabled=true, but gRPC is not available "
            "on this platform — the module will be ignored");
    }
#endif

    return cfg;
}

} // namespace axiom::core