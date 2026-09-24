// src/core/application.cpp

#include "core/application.h"
#include "core/logger.h"
#include "core/paths.h"

#include "modules/event_logger/event_logger_module.h"
#include "modules/filesystem_watcher/filesystem_watcher_module.h"
#include "modules/inventory/inventory_module.h"
#include "storage/migrations.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <iostream>

namespace axiom::core {

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
    return o;
}

Application::Application(Options opts) : opts_(std::move(opts)) {}

Application::~Application() {
    shutdown();
}

namespace {

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

bool Application::init() {
    const std::string config_path = resolve_config_path(opts_.config_path);
    config_ = Config::load(config_path);
    opts_.config_path = config_path;

    Logger::init(config_.logging.dir, config_.logging.file, config_.logging.level);
    initialized_ = true;

    SPDLOG_INFO("AxiomDLP Agent v0.1.0");
    SPDLOG_INFO("Config loaded from: {}", opts_.config_path);
    for (const auto& w : config_.load_warnings) {
        SPDLOG_WARN("Config: {}", w);
    }

    database_ = std::make_unique<Database>();
    if (!database_->open(config_.storage.db_path)) {
        SPDLOG_ERROR("Failed to open database at '{}'",
                     config_.storage.db_path);
        return false;
    }

    if (!Migrations::apply(*database_)) {
        SPDLOG_ERROR("Schema migration failed");
        return false;
    }

    file_store_ = std::make_unique<FileStore>(*database_);
    if (!file_store_->initialize_schema()) {
        SPDLOG_ERROR("FileStore::initialize_schema failed");
        return false;
    }

    event_bus_ = std::make_unique<EventBus>();

    module_context_ = std::make_unique<ModuleContext>(
        config_, *event_bus_, *file_store_);

    module_manager_ = std::make_unique<ModuleManager>();

    module_manager_->register_module(std::make_unique<FilesystemWatcherModule>());
    module_manager_->register_module(std::make_unique<InventoryModule>());
    module_manager_->register_module(std::make_unique<EventLoggerModule>());

    if (!module_manager_->initialize_all(*module_context_)) {
        SPDLOG_ERROR("Some modules failed to initialize; continuing anyway");
    }

    return true;
}

void Application::shutdown() {
    if (!initialized_) return;

    SPDLOG_INFO("Application shutting down");

    service_.reset();
    module_manager_.reset();
    module_context_.reset();
    event_bus_.reset();
    file_store_.reset();
    database_.reset();

    Logger::shutdown();
    initialized_ = false;
}

int Application::execute() {
    if (opts_.install || opts_.uninstall) {
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

    service_ = make_service(default_service_name());
    service_->set_module_manager(module_manager_.get());
    const int rc = service_->run();

    SPDLOG_INFO("Service returned rc={}", rc);
    return rc;
}

} // namespace axiom::core