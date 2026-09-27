// src/core/module_context.h

#pragma once

#include "core/config.h"
#include "core/event_bus.h"

namespace axiom::core {

class Database;
class FileStore;
class StateStore;

struct ModuleContext {
    const Config& config;
    EventBus&     event_bus;
    FileStore&    file_store;
    StateStore&   state_store;
    Database&     database;

    ModuleContext(const Config& c,
                  EventBus&     bus,
                  FileStore&    store,
                  StateStore&   state,
                  Database&     db)
        : config(c), event_bus(bus), file_store(store), state_store(state), database(db) {}
};

} // namespace axiom::core