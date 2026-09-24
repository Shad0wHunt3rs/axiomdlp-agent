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

// src/core/module_context.h

#pragma once

#include "core/config.h"
#include "core/event_bus.h"

namespace axiom::core {

// Forward-declared: only used by reference here.
class MetadataStore;

struct ModuleContext {
    const Config&  config;
    EventBus&      event_bus;
    MetadataStore& metadata_store;   // shared storage for any module

    ModuleContext(const Config& c,
                  EventBus& bus,
                  MetadataStore& store)
        : config(c), event_bus(bus), metadata_store(store) {}
};

} // namespace axiom::core