// src/main.cpp

#include "core/config.h"
#include "core/logger.h"

#include <spdlog/spdlog.h>

int main() {
    // 1. Load config first — logging depends on its values.
    axiom::core::Config cfg =
        axiom::core::Config::load("config/agent.default.json");

    // 2. Initialize logging from config values.
    axiom::core::Logger::init(cfg.logging.dir,
                              cfg.logging.file,
                              cfg.logging.level);

    // 3. Now that logging is up, replay any config warnings.
    SPDLOG_INFO("AxiomDLP Agent v0.1.0 starting");
    SPDLOG_INFO("Log level: {}", cfg.logging.level);
    SPDLOG_INFO("Log file:  {}/{}", cfg.logging.dir, cfg.logging.file);

    for (const auto& w : cfg.load_warnings) {
        SPDLOG_WARN("Config: {}", w);
    }

    SPDLOG_INFO("AxiomDLP Agent shutting down");

    axiom::core::Logger::shutdown();
    return 0;
}