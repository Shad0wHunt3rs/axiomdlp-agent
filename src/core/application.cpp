// src/core/application.cpp

#include "core/application.h"
#include "core/logger.h"
#include "core/paths.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <iostream>

namespace axiom::core {

Application::Options Application::parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {   //  It begins at index 1 because index 0 always points to the path of the executing binary itself.
        const char* a = argv[i];
        if (std::strcmp(a, "--install") == 0)         { o.install   = true; }
        else if (std::strcmp(a, "--uninstall") == 0) { o.uninstall = true; }
        else if (std::strcmp(a, "--foreground") == 0){ o.foreground= true; }
        else if (std::strcmp(a, "--config") == 0 && i + 1 < argc) {
            o.config_path = argv[++i];
        }
        else {
            std::cerr << "unknown argument: " << a << "\n";
        }
    }
    if (o.config_path.empty()) {
        // Try ./config/agent.default.json first (dev), then platform default.
        o.config_path = "config/agent.default.json";
    }
    return o;
}

Application::Application(Options opts) : opts_(std::move(opts)) {}

Application::~Application() {
    shutdown();
}

bool Application::init() {
    // 1. Config
    config_ = Config::load(opts_.config_path);

    // 2. Logging
    Logger::init(config_.logging.dir, config_.logging.file, config_.logging.level);
    initialized_ = true;

    SPDLOG_INFO("AxiomDLP Agent v0.1.0");
    SPDLOG_INFO("Config loaded from: {}", opts_.config_path);
    for (const auto& w : config_.load_warnings) {
        SPDLOG_WARN("Config: {}", w);
    }

    return true;
}

void Application::shutdown() {
    if (!initialized_) return;
    SPDLOG_INFO("Application shutting down");
    Logger::shutdown();
    initialized_ = false;
}

int Application::execute() {
    if (!init()) {
        return 1;
    }

    // Install / uninstall take priority over running.
    if (opts_.install) {
        return install_service(paths::executable_path(), default_service_name());
    }
    if (opts_.uninstall) {
        return uninstall_service(default_service_name());
    }

    // Normal run.
    service_ = make_service(default_service_name());
    const int rc = service_->run();

    SPDLOG_INFO("Service returned rc={}", rc);
    return rc;
}

} // namespace axiom::core