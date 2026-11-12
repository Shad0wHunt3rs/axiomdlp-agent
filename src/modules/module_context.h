// src/modules/module_context.h
//
// ModuleContext is the shared dependency container passed to every module
// at initialize() time. Holds references to config, event bus, storage,
// shutdown controller, and the request client that reaches the Core.

#pragma once

#include "foundation/config.h"
#include "foundation/shutdown.h"
#include "events/event_bus.h"
#include "net/i_request_client.h"

namespace axiom::core {

class Database;
class FileStore;
class StateStore;

struct ModuleContext {
    const Config&       config;
    EventBus&           event_bus;
    FileStore&          file_store;
    StateStore&         state_store;
    Database&           database;
    ShutdownController& shutdown;

    // Nullable until Application wires the transport in. Modules that
    // talk to the Core (agent_sync, content_scanner) must null-check
    // before use. The #else branch of Application::init() — when MQTT
    // is not built — passes nullptr here.
    IRequestClient*     request_client = nullptr;

    ModuleContext(const Config&         c,
                  EventBus&             bus,
                  FileStore&            store,
                  StateStore&           state,
                  Database&             db,
                  ShutdownController&   sd,
                  IRequestClient*       rc = nullptr)
        : config(c),
          event_bus(bus),
          file_store(store),
          state_store(state),
          database(db),
          shutdown(sd),
          request_client(rc) {}
};

} // namespace axiom::core