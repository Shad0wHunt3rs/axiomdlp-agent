// src/features/websocket_transport/transport_module.cpp

#include "features/websocket_transport/transport_module.h"
#include "storage/state_repository.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <string>
#include <thread>

namespace axiom::core {

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

std::string to_ws_scheme(const std::string& http_url) {
    if (http_url.rfind("https://", 0) == 0) {
        return "wss://" + http_url.substr(8);
    }
    if (http_url.rfind("http://", 0) == 0) {
        return "ws://" + http_url.substr(7);
    }
    return http_url;
}

} // namespace

bool TransportModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    const auto& tcfg = ctx_->config.modules.transport;
    const auto& srv  = ctx_->config.server;

    if (!tcfg.enabled) {
        SPDLOG_INFO("transport module: disabled in config");
        return true;
    }

    if (srv.api_token.empty()) {
        SPDLOG_ERROR("transport module: server.api_token is empty");
        return false;
    }

    api_token_ = srv.api_token;

    if (!tcfg.server_url.empty()) {
        server_url_ = tcfg.server_url;
    } else if (!srv.base_url.empty()) {
        server_url_ = to_ws_scheme(srv.base_url) + "/api/v1/agent/ws";
    } else {
        SPDLOG_ERROR("transport module: no server_url and no server.base_url");
        return false;
    }

    reconnect_backoff_sec_ = srv.reconnect_backoff_seconds;
    reconnect_backoff_max_ = srv.reconnect_backoff_max_seconds;

    SPDLOG_INFO("transport module: initialized "
                "(url={}, backoff={}s..{}s)",
                server_url_, reconnect_backoff_sec_, reconnect_backoff_max_);
    return true;
}

bool TransportModule::start() {
    if (!ctx_->config.modules.transport.enabled) {
        return true;
    }

    setup_default_handlers();

    stop_.store(false, std::memory_order_relaxed);
    manager_thread_ = std::thread([this] { manager_loop(); });
    running_.store(true, std::memory_order_relaxed);

    SPDLOG_INFO("transport module: started");
    return true;
}

void TransportModule::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // If this is a graceful shutdown (Ctrl+C, service stop, fatal
    // error), send a "goodbye" message to the server BEFORE closing
    // the socket. The server logs it and can correlate the shutdown
    // with the close frame that follows.
    //
    // On auth-revoked shutdowns, the token is already dead — no point
    // in trying to send anything.
    const ShutdownReason reason = ctx_->shutdown.reason();
    const bool want_goodbye =
        reason == ShutdownReason::UserRequested ||
        reason == ShutdownReason::FatalError   ||
        reason == ShutdownReason::ConfigError;

    if (want_goodbye && ws_ &&
        ws_->state() == WebSocketClient::State::Connected) {

        json goodbye = {
            {"type", "agent_shutdown"},
            {"payload", {
                {"reason",   to_string(reason)},
                {"agent_id", agent_id_}
            }}
        };

        SPDLOG_INFO("transport module: sending goodbye (reason={})",
                    to_string(reason));
        (void)ws_->send(goodbye.dump(), OutboundQueue::Priority::Critical);

        // Give the write pump a moment to flush the goodbye before
        // WebSocketClient::stop() sends the close frame.
        std::this_thread::sleep_for(200ms);
    }

    stop_.store(true, std::memory_order_relaxed);
    if (manager_thread_.joinable()) {
        manager_thread_.join();
    }

    if (ws_) {
        ws_->stop();
        ws_.reset();
    }
    router_.reset();

    SPDLOG_INFO("transport module: stopped");
}

void TransportModule::shutdown() {
    SPDLOG_INFO("transport module: shutting down");
}

HealthStatus TransportModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!ctx_->config.modules.transport.enabled) return HealthStatus::ok("disabled");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("not running");
    if (!ws_) return HealthStatus::degraded("waiting for agent_id");
    switch (ws_->state()) {
        case WebSocketClient::State::Connected:    return HealthStatus::ok("connected");
        case WebSocketClient::State::Connecting:   return HealthStatus::degraded("connecting");
        case WebSocketClient::State::Reconnecting: return HealthStatus::degraded("reconnecting");
        case WebSocketClient::State::Disconnected: return HealthStatus::failed("disconnected");
    }
    return HealthStatus::unknown("unknown state");
}

void TransportModule::manager_loop() {
    while (!stop_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;

        if (auto saved = ctx_->state_store.get("agent_id")) {
            try {
                int id = std::stoi(*saved);
                if (id > 0) {
                    agent_id_ = id;
                    break;
                }
            } catch (...) {
                // fall through to sleep
            }
        }
        std::this_thread::sleep_for(1s);
    }

    if (stop_.load(std::memory_order_relaxed)) return;
    if (ctx_->shutdown.is_requested()) return;

    if (agent_id_ <= 0) {
        SPDLOG_WARN("transport module: agent_id never became available; "
                    "not connecting");
        return;
    }

    const std::string full_url =
        server_url_ + "?agent_id=" + std::to_string(agent_id_);
    SPDLOG_INFO("transport module: connecting to {}", full_url);

    ws_ = std::make_unique<WebSocketClient>();
    ws_->set_bearer_token(api_token_);
    ws_->set_backoff(reconnect_backoff_sec_, reconnect_backoff_max_);
    ws_->set_on_message([this](std::string_view m) { on_ws_message(m); });
    ws_->set_on_state_change(
        [this](WebSocketClient::State s) { on_ws_state_change(s); });

    if (!ws_->start(full_url)) {
        SPDLOG_ERROR("transport module: failed to start WebSocket client");
        ws_.reset();
    }
}

void TransportModule::setup_default_handlers() {
    router_ = std::make_unique<MessageRouter>();

    router_->register_handler("ping", [](const json&) {
        SPDLOG_DEBUG("transport: received ping from server");
    });

    router_->register_handler("policy_update", [](const json& payload) {
        SPDLOG_INFO("transport: received policy_update: {}", payload.dump());
    });

    router_->register_handler("command", [](const json& payload) {
        SPDLOG_INFO("transport: received command: {}", payload.dump());
    });

    // The server sends this when the token that this agent is using
    // has been revoked. The agent responds by shutting down cleanly.
    router_->register_handler("token_revoked", [this](const json& payload) {
        SPDLOG_ERROR("transport: server signaled token_revoked: {} — "
                     "shutting down", payload.dump());
        if (ctx_) {
            ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        }
    });

    router_->register_handler("agent_revoked", [this](const json& payload) {
        SPDLOG_ERROR("transport: server signaled agent_revoked: {} — "
                     "shutting down", payload.dump());
        if (ctx_) {
            ctx_->shutdown.request(ShutdownReason::AgentRevoked);
        }
    });
}

void TransportModule::send_message(const json& msg) {
    if (!ws_) return;
    ws_->send(msg.dump(), OutboundQueue::Priority::Normal);
}

void TransportModule::register_handler(const std::string& type,
                                       MessageRouter::Handler handler) {
    if (router_) {
        router_->register_handler(type, std::move(handler));
    }
}

void TransportModule::on_ws_message(std::string_view raw) {
    if (router_) {
        router_->dispatch(raw);
    }
}

void TransportModule::on_ws_state_change(WebSocketClient::State s) {
    const char* name = "unknown";
    switch (s) {
        case WebSocketClient::State::Disconnected: name = "disconnected"; break;
        case WebSocketClient::State::Connecting:   name = "connecting";   break;
        case WebSocketClient::State::Connected:    name = "connected";    break;
        case WebSocketClient::State::Reconnecting: name = "reconnecting"; break;
    }
    SPDLOG_INFO("transport module: state = {}", name);
}

} // namespace axiom::core