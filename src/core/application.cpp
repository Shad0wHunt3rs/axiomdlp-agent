// src/core/application.cpp

#include "core/application.h"
#include "core/logger.h"
#include "core/paths.h"

#include "modules/example/example_module.h"
#include "modules/event_logger/event_logger_module.h"
#include "modules/filesystem/filesystem_module.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
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
    // config_path is resolved later, once we know the platform default.
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
// Config path resolution
// ----------------------------------------------------------------

namespace {

// Choose the config file to load:
//   1. explicit --config <path> if provided
//   2. ./config/agent.default.json if it exists (dev workflow)
//   3. the platform default (/etc/axiomdlp/agent.json, ...)
//
// Step 3 fixes the "config not found when running as a service" bug:
// systemd runs the process with CWD=/, so the dev-relative path never
// resolves.
std::string resolve_config_path(const std::string& explicit_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!explicit_path.empty()) {
        return explicit_path;
    }

    const std::string dev_path = "config/agent.default.json";
    if (fs::exists(dev_path, ec)) {
        return dev_path;
    }

    return paths::default_config_path();
}

} // namespace

// ----------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------

bool Application::init() {
    // 1. Resolve and load config.
    const std::string config_path = resolve_config_path(opts_.config_path);
    config_ = Config::load(config_path);
    opts_.config_path = config_path;   // remember for logging

    // 2. Initialize logging.
    Logger::init(config_.logging.dir, config_.logging.file, config_.logging.level);
    initialized_ = true;

    SPDLOG_INFO("AxiomDLP Agent v0.1.0");
    SPDLOG_INFO("Config loaded from: {}", opts_.config_path);
    for (const auto& w : config_.load_warnings) {
        SPDLOG_WARN("Config: {}", w);
    }

    // 3. Create the event bus.
    event_bus_ = std::make_unique<EventBus>();

    // 4. Create the module context (needs the bus).
    module_context_ = std::make_unique<ModuleContext>(config_, *event_bus_);

    // 5. Create the module manager.
    module_manager_ = std::make_unique<ModuleManager>();

    // 6. Register modules.
    //    Registration order = start order. Reverse for stop.
    //    The event_logger is registered last so it starts after
    //    producers, and stops before them.
    module_manager_->register_module(
        std::make_unique<ExampleModule>());

    module_manager_->register_module(
        std::make_unique<FilesystemModule>());

    module_manager_->register_module(
        std::make_unique<EventLoggerModule>());

    // 7. Initialize all modules with the shared context.
    //
    //    Fail-safe by design: one module failing to initialize does
    //    not stop the agent. The ModuleManager logs which modules
    //    failed; we simply note the summary here. This returns true
    //    intentionally - a partial agent is better than no agent.
    if (!module_manager_->initialize_all(*module_context_)) {
        SPDLOG_ERROR("Some modules failed to initialize; continuing anyway");
    }

    return true;
}

// ----------------------------------------------------------------
// Shutdown
// ----------------------------------------------------------------

void Application::shutdown() {
    if (!initialized_) return;

    SPDLOG_INFO("Application shutting down");
    Logger::shutdown();
    initialized_ = false;
}

// ----------------------------------------------------------------
// Execute
// ----------------------------------------------------------------

int Application::execute() {
    // Install / uninstall do not need config, modules, or the event
    // bus. Handle them before full init so a one-shot command doesn't
    // create log directories or start the module system.
    if (opts_.install || opts_.uninstall) {
        // Minimal logger so the messages are still visible.
        Logger::init("logs", "install.log", "info");
        initialized_ = true;

        int rc = 0;
        if (opts_.install) {
            SPDLOG_INFO("Installing service...");
            rc = install_service(paths::executable_path(), default_service_name());
        } else {
            SPDLOG_INFO("Uninstalling service...");
            rc = uninstall_service(default_service_name());
        }

        Logger::shutdown();
        initialized_ = false;
        return rc;
    }

    if (!init()) {
        return 1;
    }

    // Normal run.
    service_ = make_service(default_service_name());
    service_->set_module_manager(module_manager_.get());
    const int rc = service_->run();

    SPDLOG_INFO("Service returned rc={}", rc);
    return rc;
}

} // namespace axiom::core