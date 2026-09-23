// src/core/application.h
//
// Application owns config, logging, the module manager, and the service
// lifecycle.

#pragma once

#include "core/config.h"
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

    std::unique_ptr<ServiceContext> service_context_;
    std::unique_ptr<ModuleManager>  module_manager_;
    std::unique_ptr<IService>       service_;

    bool initialized_ = false;
};

} // namespace axiom::core