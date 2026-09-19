// src/core/application.h
//
// Application owns config, logging, and the service lifecycle.
// main() shrinks to: parse args -> create Application -> execute().

#pragma once

#include "core/config.h"
#include "core/service.h"

#include <memory>
#include <string>

namespace axiom::core {

class Application {
public:
    struct Options {
        bool install     = false;
        bool uninstall   = false;
        bool foreground  = false;
        std::string config_path;   // empty -> use default
    };

    // Parse argv. Unknown args produce a warning on stderr but do not fail.
    static Options parse_args(int argc, char** argv);

    explicit Application(Options opts);
    ~Application();

    // Perform the requested action. Returns an exit code.
    int execute();

private:
    bool init();
    void shutdown();

    Options opts_;
    Config  config_;
    std::unique_ptr<IService> service_;
    bool initialized_ = false;
};

} // namespace axiom::core