// src/modules/example/example_module.h
//
// ExampleModule is a minimal module used to prove the module system
// and the event bus work end-to-end.
//
// Lifecycle:
//   initialize() → logs, stores context
//   start()      → subscribes to a test event, publishes one
//   stop()       → unsubscribes
//   shutdown()   → logs

#pragma once

#include "core/event_bus.h"
#include "modules/module_base.h"

#include <string>

namespace axiom::core {

class ExampleModule final : public ModuleBase {
public:
    std::string name() const override {
        return "example";
    }

    bool initialize(ServiceContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;

private:
    SubscriptionId subscription_id_ = 0;
};

} // namespace axiom::core