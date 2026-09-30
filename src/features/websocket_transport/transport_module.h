// src/features/websocket_transport/transport_module.h
//
// TransportModule owns the WebSocket connection to the AxiomDLP Core
// server. It reuses server.api_token (same bearer token) and derives
// its URL from server.base_url unless transport.server_url is
// explicitly set.
//
// TLS is always verified on wss:// connections.
//
// Connection is deferred until agent_sync has enrolled and written
// agent_id to the state store — the server needs agent_id to address
// messages to this specific agent.

#pragma once

#include "modules/module_base.h"
#include "net/message_router.h"
#include "net/websocket_client.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

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

    void manager_loop();
    void setup_default_handlers();

    std::unique_ptr<WebSocketClient> ws_;
    std::unique_ptr<MessageRouter>   router_;

    std::string server_url_;
    std::string api_token_;
    int         reconnect_backoff_sec_ = 10;
    int         reconnect_backoff_max_ = 300;

    int agent_id_ = 0;

    std::thread       manager_thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
};

} // namespace axiom::core