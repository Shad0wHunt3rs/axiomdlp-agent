// src/modules/transport/transport_module.cpp

#include "modules/transport/transport_module.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace axiom::core {

using json = nlohmann::json;

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------

bool TransportModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.transport.enabled) {
        SPDLOG_INFO("transport module: disabled in config");
        return true;
    }

    const auto& cfg = ctx_->config.modules.transport;

    server_url_            = cfg.server_url;
    reconnect_backoff_sec_ = cfg.reconnect_backoff_seconds;
    reconnect_backoff_max_ = cfg.reconnect_backoff_max_seconds;
    verify_tls_            = cfg.verify_tls;

    SPDLOG_INFO("transport module: initialized (server: {}, verify_tls: {})",
                server_url_, verify_tls_ ? "true" : "false");
    return true;
}

bool TransportModule::start() {
    if (!ctx_->config.modules.transport.enabled) {
        return true;
    }

    router_ = std::make_unique<MessageRouter>();

    ws_ = std::make_unique<WebSocketClient>();
    ws_->set_backoff(reconnect_backoff_sec_, reconnect_backoff_max_);
    ws_->set_verify_tls(verify_tls_);
    ws_->set_on_message(
        [this](std::string_view msg) { on_ws_message(msg); });
    ws_->set_on_state_change(
        [this](WebSocketClient::State s) { on_ws_state_change(s); });

    if (!ws_->start(server_url_)) {
        SPDLOG_ERROR("transport module: failed to start WebSocket client");
        ws_.reset();
        router_.reset();
        return false;
    }

    running_ = true;
    SPDLOG_INFO("transport module: started");
    return true;
}

void TransportModule::stop() {
    if (!running_) {
        return;
    }

    if (ws_) {
        ws_->stop();
        ws_.reset();
    }

    router_.reset();
    running_ = false;
    SPDLOG_INFO("transport module: stopped");
}

void TransportModule::shutdown() {
    SPDLOG_INFO("transport module: shutting down");
}

HealthStatus TransportModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!ctx_->config.modules.transport.enabled) return HealthStatus::ok("disabled");
    if (!running_) return HealthStatus::failed("transport not running");
    if (!ws_) return HealthStatus::failed("no websocket client");

    switch (ws_->state()) {
        case WebSocketClient::State::Connected:    return HealthStatus::ok("connected");
        case WebSocketClient::State::Connecting:   return HealthStatus::degraded("connecting");
        case WebSocketClient::State::Reconnecting: return HealthStatus::degraded("reconnecting");
        case WebSocketClient::State::Disconnected: return HealthStatus::failed("disconnected");
    }
    return HealthStatus::unknown("unknown state");
}

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------

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

// ----------------------------------------------------------------
// WebSocket callbacks
// ----------------------------------------------------------------

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