// src/core/application.h

#pragma once

#include "core/config.h"
#include "core/event_bus.h"
#include "core/module_context.h"
#include "core/module_manager.h"
#include "core/service.h"
#include "storage/database.h"
#include "storage/file_store.h"
#include "storage/state_store.h"

#include <memory>
#include <string>

namespace axiom::core {

class Application {
public:
    struct Options {
        bool install    = false;
        bool uninstall  = false;
        bool foreground = false;
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

    std::unique_ptr<Database>   database_;
    std::unique_ptr<FileStore>  file_store_;
    std::unique_ptr<StateStore> state_store_;
    std::unique_ptr<EventBus>   event_bus_;

    std::unique_ptr<ModuleContext> module_context_;
    std::unique_ptr<ModuleManager> module_manager_;
    std::unique_ptr<IService>      service_;

    bool initialized_ = false;
};

} // namespace axiom::core