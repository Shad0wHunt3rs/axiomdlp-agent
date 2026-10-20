// src/foundation/application.cpp

#include "foundation/application.h"
#include "foundation/logger.h"
#include "platform/paths.h"

#include "features/agent_sync/agent_sync_module.h"
#include "features/content_scanner/content_scanner_module.h"
#include "features/event_logger/event_logger_module.h"
#include "features/filesystem_watcher/filesystem_watcher_module.h"
#include "features/inventory/inventory_module.h"
#include "features/websocket_transport/transport_module.h"
#ifdef AXIOM_HAVE_GRPC
#include "features/grpc_transport/grpc_transport_module.h"
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

Application::~Application() {
    shutdown();
}

namespace {

std::string resolve_config_path(const std::string& explicit_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!explicit_path.empty()) return explicit_path;

    const std::string dev_path = "config/agent.default.json";
    if (fs::exists(dev_path, ec)) return dev_path;

    return paths::default_config_path();
}

} // namespace

bool Application::init() {
    // 1. Create the shutdown controller FIRST. Everything else that
    //    might fail and need to report a reason uses it.
    shutdown_controller_ = std::make_unique<ShutdownController>();

    // 2. Resolve and load config.
    const std::string config_path = resolve_config_path(opts_.config_path);
    config_ = Config::load(config_path);
    opts_.config_path = config_path;

    // 3. Initialize logging.
    Logger::init(config_.logging.dir, config_.logging.file, config_.logging.level);
    initialized_ = true;

    SPDLOG_INFO("AxiomDLP Agent v0.1.0");
    SPDLOG_INFO("Config loaded from: {}", opts_.config_path);
    for (const auto& w : config_.load_warnings) {
        SPDLOG_WARN("Config: {}", w);
    }

    // 4. Open the database.
    database_ = std::make_unique<Database>();
    if (!database_->open(config_.storage.db_path)) {
        SPDLOG_ERROR("Failed to open database at '{}'",
                     config_.storage.db_path);
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }

    // 5. Initialize schema.
    if (!Migrations::apply(*database_)) {
        SPDLOG_ERROR("Schema migration failed");
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }

    // 6. File store.
    file_store_ = std::make_unique<FileStore>(*database_);
    if (!file_store_->initialize_schema()) {
        SPDLOG_ERROR("FileStore::initialize_schema failed");
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }

    // 7. State store.
    state_store_ = std::make_unique<StateStore>(*database_);
    if (!state_store_->initialize_schema()) {
        SPDLOG_ERROR("StateStore::initialize_schema failed");
        shutdown_controller_->request(ShutdownReason::FatalError);
        return false;
    }

    // 8. Event bus.
    event_bus_ = std::make_unique<EventBus>();

    // 9. Module context (now includes the shutdown controller).
    module_context_ = std::make_unique<ModuleContext>(
        config_, *event_bus_, *file_store_, *state_store_, *database_,
        *shutdown_controller_);

    // 10. Module manager.
    module_manager_ = std::make_unique<ModuleManager>();

    // 11. Register modules. Stop/shutdown run in reverse order.
    module_manager_->register_module(std::make_unique<FilesystemWatcherModule>());
    module_manager_->register_module(std::make_unique<InventoryModule>());
    module_manager_->register_module(std::make_unique<EventLoggerModule>());
    module_manager_->register_module(std::make_unique<TransportModule>());
    module_manager_->register_module(std::make_unique<AgentSyncModule>());
    module_manager_->register_module(std::make_unique<ContentScannerModule>());

#ifdef AXIOM_HAVE_GRPC
    module_manager_->register_module(std::make_unique<GrpcTransportModule>());
#endif

    // 12. Initialize all modules.
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
    service_->set_shutdown_controller(shutdown_controller_.get());

    const int rc = service_->run();

    SPDLOG_INFO("Service returned rc={} (reason={})",
                rc, to_string(shutdown_controller_->reason()));
    return rc;
}

} // namespace axiom::core