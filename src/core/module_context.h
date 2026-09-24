// src/core/module_context.h
//
// ModuleContext holds references to all shared subsystems that modules
// may need. It is created once at startup by the Application, passed to
// the ModuleManager, and distributed to every module during
// initialize().
//
// Modules are the only consumers of this context. The Service and the
// Application do not use it directly.
//
// In later milestones, more subsystems will be added here (storage,
// policy engine, transport).

#pragma once

#include "core/config.h"
#include "core/event_bus.h"

namespace axiom::core {

struct ModuleContext {
    // The loaded configuration. Read-only for modules.
    const Config& config;

    // The event bus. Modules publish and subscribe through this.
    EventBus& event_bus;

    ModuleContext(const Config& c, EventBus& bus)
        : config(c), event_bus(bus) {}
};

} // namespace axiom::core