// src/core/paths.h
//
// Platform-specific filesystem paths the agent needs.
// Each function has one implementation per platform, selected at build time.

#pragma once

#include <string>

namespace axiom::core::paths {

// Full path to the running executable (e.g., /usr/local/bin/axiomdlp-agent).
std::string executable_path();

// Directory containing the running executable.
std::string executable_dir();

// Default config file path for this platform.
std::string default_config_path();

// Default log directory for this platform.
std::string default_log_dir();

// Default data directory (SQLite DB, cached policies, etc.).
std::string default_data_dir();

} // namespace axiom::core::paths