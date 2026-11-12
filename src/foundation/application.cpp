// src/foundation/application.cpp

#include "foundation/application.h"
#include "foundation/logger.h"
#include "platform/paths.h"

#include "features/agent_sync/agent_sync_module.h"
#include "features/content_scanner/content_scanner_module.h"
#include "features/event_logger/event_logger_module.h"
#include "features/filesystem_watcher/filesystem_watcher_module.h"
#include "features/inventory/inventory_module.h"
#ifdef AXIOM_HAVE_MQTT
#  include "features/mqtt_transport/mqtt_transport_module.h"
#endif
#include "storage/migrations.h"
#include "storage/state_repository.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <iostream>

namespace axiom::core {

Application::Options Application::parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--install") == 0)          { o.install    = true; }
        else if (std::strcmp(a, "--uninstall") == 0)   { o.uninstall  = true; }
        else if (std::strcmp(a, "--foreground") == 0)  { o.foreground = true; }
        else if (std::strcmp(a, "--config") == 0 && i + 1 < argc) {
            o.config_path = argv[++i];
        } else {
            std::cerr << "unknown argument: " << a << "\n";
        }
    }
    return o;
}

Application::Application(Options opts) : opts_(std::move(opts)) {}
Application::~Application() { shutdown(); }

namespace {
std::string resolve_config_path(const std::string& explicit_path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!explicit_path.empty()) return explicit_path;
    const std::string dev_path = "config/agent.default.json";
    if (fs::exists(fs::path(dev_path), ec)) return dev_path;
    return paths::default_config_path();
}
} // namespace

bool Application::init() {
    shutdown_controller_ = std::make_unique<ShutdownController>();
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
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }
    if (!Migrations::apply(*database_)) {
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }
    file_store_ = std::make_unique<FileStore>(*database_);
    if (!file_store_->initialize_schema()) {
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }
    state_store_ = std::make_unique<StateStore>(*database_);
    if (!state_store_->initialize_schema()) {
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }

    event_bus_ = std::make_unique<EventBus>();

#ifdef AXIOM_HAVE_MQTT
    // MQTT transport module is constructed first so its IRequestClient
    // interface is available via ModuleContext before feature modules
    // are instantiated.
    auto mqtt_module = std::make_unique<MqttTransportModule>();
    IRequestClient* mqtt_client = mqtt_module->as_request_client();

    module_context_ = std::make_unique<ModuleContext>(
        config_, *event_bus_, *file_store_, *state_store_,
        *database_, *shutdown_controller_, mqtt_client);
    module_manager_ = std::make_unique<ModuleManager>();

    module_manager_->register_module(std::make_unique<FilesystemWatcherModule>());
    module_manager_->register_module(std::make_unique<InventoryModule>());
    module_manager_->register_module(std::make_unique<EventLoggerModule>());
    module_manager_->register_module(std::move(mqtt_module));
    module_manager_->register_module(std::make_unique<AgentSyncModule>());
    module_manager_->register_module(std::make_unique<ContentScannerModule>());
#else
    SPDLOG_WARN("MQTT transport disabled at build time (AXIOM_HAVE_MQTT=OFF); "
                "server-push and MQTT request paths are unavailable");
    module_context_ = std::make_unique<ModuleContext>(
        config_, *event_bus_, *file_store_, *state_store_,
        *database_, *shutdown_controller_, nullptr);
    module_manager_ = std::make_unique<ModuleManager>();

    module_manager_->register_module(std::make_unique<FilesystemWatcherModule>());
    module_manager_->register_module(std::make_unique<InventoryModule>());
    module_manager_->register_module(std::make_unique<EventLoggerModule>());
    module_manager_->register_module(std::make_unique<AgentSyncModule>());
    module_manager_->register_module(std::make_unique<ContentScannerModule>());
#endif

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
    state_store_.reset();
    file_store_.reset();
    database_.reset();
    shutdown_controller_.reset();
    Logger::shutdown();
    initialized_ = false;
}

int Application::execute() {
    if (opts_.install || opts_.uninstall) {
        Logger::init("logs", "install.log", "info");
        initialized_ = true;
        int rc = 0;
        if (opts_.install) rc = install_service(paths::executable_path(), default_service_name());
        else rc = uninstall_service(default_service_name());
        Logger::shutdown();
        initialized_ = false;
        return rc;
    }
    if (!init()) return 1;

    service_ = make_service(default_service_name());
    service_->set_module_manager(module_manager_.get());
    service_->set_shutdown_controller(shutdown_controller_.get());

    const int rc = service_->run();
    SPDLOG_INFO("Service returned rc={} (reason={})", rc, to_string(shutdown_controller_->reason()));
    return rc;
}

} // namespace axiom::core