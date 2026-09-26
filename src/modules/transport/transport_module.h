// src/modules/transport/transport_module.h
//
// TransportModule owns the WebSocket connection to the AxiomDLP Core
// server.
//
// This is a minimal version: it establishes the connection, logs
// state changes, and dispatches incoming messages to registered
// handlers. It does NOT send heartbeats, does NOT ship data, and
// does NOT persist any state.
//
// A future iteration will add heartbeats, shipping, and cursor
// persistence once the server side is defined.

#pragma once

#include "modules/module_base.h"
#include "modules/transport/message_router.h"
#include "modules/transport/websocket_client.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace axiom::core {

class TransportModule final : public ModuleBase {
public:
    std::string name() const override { return "transport"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

    // Send a message to the server. Callers use this to talk to the
    // server. Thread-safe.
    void send_message(const nlohmann::json& msg);

    // Register a handler for a specific server message type. The
    // handler is called on the WebSocket I/O thread when a matching
    // message arrives.
    void register_handler(const std::string& type, MessageRouter::Handler handler);

private:
    void on_ws_message(std::string_view raw);
    void on_ws_state_change(WebSocketClient::State s);

    std::unique_ptr<WebSocketClient> ws_;
    std::unique_ptr<MessageRouter>   router_;

    std::string server_url_;
    int reconnect_backoff_sec_ = 5;
    int reconnect_backoff_max_ = 60;

    bool running_ = false;
};

} // namespace axiom::core