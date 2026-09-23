// src/core/service_context.h
//
// ServiceContext holds references to all shared subsystems that modules
// may need. It is created once at startup and passed to every module's
// initialize() method.
//
// This is the ONLY way modules access shared state — they don't reach
// for globals or singletons.
//
// In later milestones, more subsystems will be added here (event bus,
// storage, policy engine, transport). Modules receive the whole context
// and use only what they need.

#pragma once

#include "core/config.h"

namespace axiom::core {

struct ServiceContext {
    // The loaded configuration. Read-only for modules.
    const Config& config;
};

} // namespace axiom::core
