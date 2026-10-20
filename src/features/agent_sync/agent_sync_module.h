// src/features/agent_sync/agent_sync_module.h
#pragma once

#include "modules/module_base.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <optional>
#include <nlohmann/json.hpp>

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
    static std::string detect_arch();

    std::string base_url_;
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