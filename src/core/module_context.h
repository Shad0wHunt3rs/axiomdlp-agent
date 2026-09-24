// src/core/module_context.h

#pragma once

#include "core/config.h"
#include "core/event_bus.h"

namespace axiom::core {

class FileStore;

struct ModuleContext {
    const Config& config;
    EventBus&     event_bus;
    FileStore&    file_store;

    ModuleContext(const Config& c, EventBus& bus, FileStore& store)
        : config(c), event_bus(bus), file_store(store) {}
};

} // namespace axiom::core