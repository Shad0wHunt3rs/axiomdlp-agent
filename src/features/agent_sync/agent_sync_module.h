// src/modules/agent_sync/agent_sync_module.h
//
// AgentSyncModule is the HTTP channel between the agent and the
// AxiomDLP Core server. It handles exactly two operations:
//
//   1. Enrollment — POST /api/v1/agent/register (once at startup)
//   2. Heartbeat  — POST /api/v1/agent/heartbeat (periodic)
//
// Both use the "axdlp_user_..." bearer token from config. The
// agent_id returned at enrollment is persisted in the StateStore so
// that it survives agent restarts.
//
// This module is deliberately separate from the existing `transport`
// module, which owns the WebSocket push channel. HTTP request/response
// and WebSocket push are different concerns with different lifecycles.

#pragma once

#include "modules/module_base.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace axiom::core {

class HttpClient;

class AgentSyncModule final : public ModuleBase {
public:
    AgentSyncModule();
    ~AgentSyncModule() override;

    std::string name() const override { return "agent_sync"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    // Main worker loop.
    void sync_loop();

    // One-shot enrollment. Returns true on success.
    bool enroll();

    // One heartbeat. Returns true if the call succeeded and the agent
    // should keep heartbeating.
    bool heartbeat();

    // Small platform-agnostic helpers.
    static std::string detect_hostname();
    static std::string detect_os();

    // Configuration (loaded in initialize()).
    std::string http_base_url_;
    std::string api_token_;
    int         heartbeat_interval_sec_ = 30;
    bool        enabled_ = true;

    // Runtime state.
    std::unique_ptr<HttpClient> http_;

    std::thread       sync_thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> needs_re_enroll_{false};

    // Populated after successful enrollment.
    int agent_id_ = 0;
};

} // namespace axiom::core