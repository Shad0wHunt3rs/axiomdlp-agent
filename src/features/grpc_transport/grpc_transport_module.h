// src/features/grpc_transport/grpc_transport_module.h
#pragma once

#include "modules/module_base.h"
#include "agent.grpc.pb.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace axiom::core {

class GrpcTransportModule final : public ModuleBase {
public:
    GrpcTransportModule();
    ~GrpcTransportModule() override;

    std::string name() const override { return "grpc_transport"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    void manager_loop();
    grpc::Status run_channel_stream();
    void handle_server_message(const axiomdlp::agent::v1::ChannelResponse& resp);

    std::string server_url_;
    std::string api_token_;
    int         agent_id_ = 0;

    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<axiomdlp::agent::v1::AgentService::Stub> stub_;

    std::thread       manager_thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
};

} // namespace axiom::core