// src/modules/transport/transport_module.h
//
// TransportModule owns the WebSocket connection to the AxiomDLP Core
// server. Supports both ws:// and wss://. Does not send heartbeats,
// ship data, or persist state — those are separate concerns.

#pragma once

#include "modules/module_base.h"
#include "net/message_router.h"
#include "net/websocket_client.h"

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

    void send_message(const nlohmann::json& msg);
    void register_handler(const std::string& type, MessageRouter::Handler handler);

private:
    void on_ws_message(std::string_view raw);
    void on_ws_state_change(WebSocketClient::State s);

    std::unique_ptr<WebSocketClient> ws_;
    std::unique_ptr<MessageRouter>   router_;

    std::string server_url_;
    int  reconnect_backoff_sec_ = 5;
    int  reconnect_backoff_max_ = 60;
    bool verify_tls_            = true;   // NEW

    bool running_ = false;
};

} // namespace axiom::core