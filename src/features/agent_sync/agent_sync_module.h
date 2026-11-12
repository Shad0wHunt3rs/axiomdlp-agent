// src/features/agent_sync/agent_sync_module.h
#pragma once

#include "modules/module_base.h"
#include "net/i_request_client.h"

#include <atomic>
#include <string>
#include <thread>

namespace axiom::core {

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
    void wait_for_transport_ready();

    static std::string detect_hostname();
    static std::string detect_os();
    static std::string detect_arch();

    IRequestClient* req_ = nullptr;   // borrowed from ModuleContext; not owned

    int  heartbeat_interval_sec_ = 60;
    bool enabled_ = true;

    std::thread       sync_thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> needs_re_enroll_{false};

    int agent_id_ = 0;
};

} // namespace axiom::core