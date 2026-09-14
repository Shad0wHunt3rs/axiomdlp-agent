// src/main.cpp

#include "core/logger.h"

#include <spdlog/spdlog.h>

int main() {
    // Initialize logging (console + rotating file).
    axiom::core::Logger::init("logs", "agent.log", "info");

    SPDLOG_INFO("AxiomDLP Agent v0.1.0 starting");
    SPDLOG_DEBUG("Debug message (won't appear at 'info' level)");
    SPDLOG_WARN("Example warning");
    SPDLOG_ERROR("Example error");

    SPDLOG_INFO("AxiomDLP Agent shutting down");

    // Flush and release the async thread pool.
    axiom::core::Logger::shutdown();
    return 0;
}