// src/modules/event_logger/event_logger_module.h
//
// EventLoggerModule subscribes to every event on the bus and logs it.
// Its only purpose is observability: it proves the pipeline works and
// gives a visible trace of everything flowing through the bus.
//
// It is a pure consumer: it never publishes events.

#pragma once

#include "events/event_bus.h"
#include "modules/module_base.h"

#include <string>

namespace axiom::core {

class EventLoggerModule final : public ModuleBase {
public:
    std::string name() const override { return "event_logger"; }

    bool start() override;
    void stop() override;

private:
    SubscriptionId subscription_id_ = 0;
};

} // namespace axiom::core
