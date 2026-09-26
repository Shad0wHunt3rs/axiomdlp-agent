// src/core/module_context.h
//
// ModuleContext holds references to all shared subsystems that modules
// may need. It is created once at startup by the Application, passed to
// the ModuleManager, and distributed to every module during
// initialize().

#pragma once

#include "core/config.h"
#include "core/event_bus.h"

namespace axiom::core {

class Database;
class FileStore;

struct ModuleContext {
    const Config& config;
    EventBus&     event_bus;
    FileStore&    file_store;
    Database&     database;

    ModuleContext(const Config& c,
                  EventBus&     bus,
                  FileStore&    store,
                  Database&     db)
        : config(c), event_bus(bus), file_store(store), database(db) {}
};

} // namespace axiom::core