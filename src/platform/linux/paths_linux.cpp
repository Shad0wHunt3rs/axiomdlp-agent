// src/platform/linux/paths_linux.cpp

#include "core/paths.h"

#include <limits.h>
#include <unistd.h>

#include <filesystem>

namespace axiom::core::paths {

std::string executable_path() {
    char buf[PATH_MAX + 1] = {0};
    ssize_t n = ::readlink("/proc/self/exe", buf, PATH_MAX);
    if (n <= 0) {
        return {};
    }
    return std::string(buf, static_cast<size_t>(n));
}

std::string executable_dir() {
    const std::string p = executable_path();
    if (p.empty()) {
        return {};
    }
    return std::filesystem::path(p).parent_path().string();
}

std::string default_config_path() {
    return "/etc/axiomdlp/agent.json";
}

std::string default_log_dir() {
    return "/var/log/axiomdlp";
}

std::string default_data_dir() {
    return "/var/lib/axiomdlp";
}

} // namespace axiom::core::paths