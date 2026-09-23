// src/core/service_context.h
//
// ServiceContext holds references to all shared subsystems that modules
// may need. It is created once at startup and passed to every module's
// initialize() method.
//
// In later milestones, more subsystems will be added here (storage,
// policy engine, transport).

#pragma once

#include "core/config.h"
#include "core/event_bus.h"

namespace axiom::core {

struct ServiceContext {
    // The loaded configuration. Read-only for modules.
    const Config& config;

    // The event bus. Modules publish and subscribe through this.
    EventBus& event_bus;

    ServiceContext(const Config& c, EventBus& bus)
        : config(c), event_bus(bus) {}
};

} // namespace axiom::core