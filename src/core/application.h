// src/core/application.h
//
// Application owns config, logging, the event bus, the module manager,
// and the service lifecycle.

#pragma once

#include "core/config.h"
#include "core/event_bus.h"
#include "core/module_manager.h"
#include "core/service.h"
#include "core/service_context.h"

#include <memory>
#include <string>

namespace axiom::core {

class Application {
public:
    struct Options {
        bool install     = false;
        bool uninstall   = false;
        bool foreground  = false;
        std::string config_path;
    };

    static Options parse_args(int argc, char** argv);

    explicit Application(Options opts);
    ~Application();

    int execute();

private:
    bool init();
    void shutdown();

    Options opts_;
    Config  config_;

    // The event bus — created before the ServiceContext.
    std::unique_ptr<EventBus> event_bus_;

    // The shared dependencies container — passed to every module.
    std::unique_ptr<ServiceContext> service_context_;

    // The module manager — owns and drives all modules.
    std::unique_ptr<ModuleManager> module_manager_;

    // The platform service.
    std::unique_ptr<IService> service_;

    bool initialized_ = false;
};

} // namespace axiom::core