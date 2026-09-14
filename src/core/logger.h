// src/core/logger.h
//
// Centralized logging setup for AxiomDLP Agent.
// Wraps spdlog with a console sink and a rotating file sink,
// running in asynchronous mode so logging never blocks the caller.

#pragma once

#include <string>

namespace axiom::core {

class Logger {
public:
    // Initialize the global logger.
    // - log_dir:  directory where log files are written (created if missing)
    // - log_file: filename inside log_dir
    // - level:    "trace", "debug", "info", "warn", "error", "critical", "off"
    static void init(const std::string& log_dir  = "logs",
                     const std::string& log_file = "agent.log",
                     const std::string& level    = "info");

    // Flush all pending messages and release resources.
    // Call this before the process exits.
    static void shutdown();
};

} // namespace axiom::core