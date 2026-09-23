// src/modules/example/example_module.h
//
// ExampleModule is a minimal module used to prove the plugin architecture.
// It implements only the methods that produce output, inheriting the rest
// from ModuleBase.
//
// Lifecycle:
//   initialize() → logs "initializing"
//   start()      → logs "starting"
//   stop()       → logs "stopping"
//   shutdown()   → logs "shutting down"

#pragma once

#include "modules/module_base.h"

#include <string>

namespace axiom::core {

class ExampleModule final : public ModuleBase {
public:
    // Required: every module must have a name.
    std::string name() const override {
        return "example";
    }

    // Override the default lifecycle methods to log each transition.
    bool initialize(ServiceContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
};

} // namespace axiom::core