// src/core/application.cpp

#include "core/application.h"
#include "core/logger.h"
#include "core/paths.h"

#include "modules/example/example_module.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <iostream>

namespace axiom::core {

// ----------------------------------------------------------------
// Argument parsing
// ----------------------------------------------------------------

Application::Options Application::parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
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
        o.config_path = "config/agent.default.json";
    }
    return o;
}

// ----------------------------------------------------------------
// Construction / destruction
// ----------------------------------------------------------------

Application::Application(Options opts) : opts_(std::move(opts)) {}

Application::~Application() {
    shutdown();
}

// ----------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------

bool Application::init() {
    // 1. Load config.
    config_ = Config::load(opts_.config_path);

    // 2. Initialize logging.
    Logger::init(config_.logging.dir, config_.logging.file, config_.logging.level);
    initialized_ = true;

    SPDLOG_INFO("AxiomDLP Agent v0.1.0");
    SPDLOG_INFO("Config loaded from: {}", opts_.config_path);
    for (const auto& w : config_.load_warnings) {
        SPDLOG_WARN("Config: {}", w);
    }

    // 3. Create the shared service context.
    service_context_ = std::make_unique<ServiceContext>(config_);

    // 4. Create the module manager.
    module_manager_ = std::make_unique<ModuleManager>();

    // 5. Register modules.
    module_manager_->register_module(
        std::make_unique<ExampleModule>());

    // 6. Initialize all modules with the shared context.
    if (!module_manager_->initialize_all(*service_context_)) {
        SPDLOG_WARN("Some modules failed to initialize");
    }

    return true;
}

// ----------------------------------------------------------------
// Shutdown
// ----------------------------------------------------------------

void Application::shutdown() {
    if (!initialized_) return;

    // Modules are stopped by the service when it exits.
    // Here we only tear down logging and clear ownership.

    SPDLOG_INFO("Application shutting down");
    Logger::shutdown();
    initialized_ = false;
}

// ----------------------------------------------------------------
// Execute
// ----------------------------------------------------------------

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
    service_->set_module_manager(module_manager_.get());
    service_->set_service_context(service_context_.get());
    const int rc = service_->run();

    SPDLOG_INFO("Service returned rc={}", rc);
    return rc;
}

} // namespace axiom::core