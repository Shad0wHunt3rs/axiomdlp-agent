// src/features/agent_sync/agent_sync_module.h
//
// AgentSyncModule is the HTTP channel between the agent and the
// AxiomDLP Core server.
//
// Auth failures (401/403) are fatal. When either the enrollment or
// heartbeat is rejected, this module calls ShutdownController with
// AuthRevoked or AgentRevoked, which stops the whole agent. There is
// no point retrying a revoked token.

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
    void sync_loop();
    bool enroll();
    bool heartbeat();

    static std::string detect_hostname();
    static std::string detect_os();

    std::string http_base_url_;
    std::string api_token_;
    int         heartbeat_interval_sec_ = 30;
    bool        enabled_ = true;

    std::unique_ptr<HttpClient> http_;

    std::thread       sync_thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> needs_re_enroll_{false};

    int agent_id_ = 0;
};

} // namespace axiom::core