// src/features/grpc_transport/grpc_transport_module.cpp
//
// Minimal gRPC transport stub. It proves that:
//   - gRPC + protobuf headers compile
//   - protoc-generated .pb.cc and .grpc.pb.cc link
//   - grpc::Channel can be instantiated
//   - AgentService::NewStub() returns a valid stub
//   - The generated Heartbeat RPC compiles end-to-end
//
// The actual RPC traffic is stubbed: this module creates a channel,
// logs the state, and then idles. Wiring the bidirectional streaming
// API to the existing message router is the next milestone.

#ifdef AXIOM_HAVE_GRPC

#include "features/grpc_transport/grpc_transport_module.h"

#include "axiomdlp/agent/v1/agent.grpc.pb.h"
#include "axiomdlp/agent/v1/agent.pb.h"

#include "storage/state_repository.h"

#include <grpcpp/grpcpp.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

namespace axiom::core {

using namespace std::chrono_literals;
using namespace axiomdlp::agent::v1;

namespace {

constexpr const char* kStateKeyAgentId = "agent_id";

std::string to_grpc_target(std::string url) {
    if (url.rfind("https://", 0) == 0) {
        // TLS is a follow-up; strip scheme so grpc can parse the
        // host:port. Production would use SslCredentials here.
        return url.substr(8);
    }
    if (url.rfind("http://", 0) == 0) {
        return url.substr(7);
    }
    return url;
}

} // namespace

GrpcTransportModule::GrpcTransportModule()  = default;
GrpcTransportModule::~GrpcTransportModule() = default;

bool GrpcTransportModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.grpc_transport.enabled) {
        SPDLOG_INFO("grpc_transport module: disabled in config");
        return true;
    }

    const auto& srv = ctx_->config.server;
    if (srv.base_url.empty() || srv.api_token.empty()) {
        SPDLOG_ERROR("grpc_transport: server.base_url or server.api_token is empty");
        return false;
    }

    server_url_ = to_grpc_target(srv.base_url);
    api_token_  = srv.api_token;

    SPDLOG_INFO("grpc_transport module: initialized (target={})", server_url_);
    return true;
}

bool GrpcTransportModule::start() {
    if (!ctx_->config.modules.grpc_transport.enabled) {
        return true;
    }

    stop_.store(false, std::memory_order_relaxed);
    manager_thread_ = std::thread([this] { manager_loop(); });
    running_.store(true, std::memory_order_relaxed);

    SPDLOG_INFO("grpc_transport module: started");
    return true;
}

void GrpcTransportModule::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    stop_.store(true, std::memory_order_relaxed);
    if (manager_thread_.joinable()) {
        manager_thread_.join();
    }

    SPDLOG_INFO("grpc_transport module: stopped");
}

void GrpcTransportModule::shutdown() {
    SPDLOG_INFO("grpc_transport module: shutting down");
}

HealthStatus GrpcTransportModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!ctx_->config.modules.grpc_transport.enabled) return HealthStatus::ok("disabled");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("not running");
    if (agent_id_ == 0) return HealthStatus::degraded("waiting for agent_id");
    return HealthStatus::degraded("stub — RPC implementation pending");
}

void GrpcTransportModule::manager_loop() {
    // Wait for agent_id (written by agent_sync after HTTP enrollment).
    while (!stop_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
            try {
                int id = std::stoi(*saved);
                if (id > 0) { agent_id_ = id; break; }
            } catch (...) {}
        }
        std::this_thread::sleep_for(1s);
    }

    if (agent_id_ <= 0) {
        SPDLOG_WARN("grpc_transport: agent_id never became available; idle");
        while (!stop_.load(std::memory_order_relaxed)) {
            if (ctx_->shutdown.is_requested()) return;
            std::this_thread::sleep_for(1s);
        }
        return;
    }

    // ---- Compile/link proof: create channel + stub ----
    SPDLOG_INFO("grpc_transport: agent_id={}, creating channel to {}",
                agent_id_, server_url_);

    auto channel = grpc::CreateChannel(server_url_, grpc::InsecureChannelCredentials());
    auto stub    = AgentService::NewStub(channel);

    SPDLOG_INFO("grpc_transport: stub created ({})",
                stub ? "ok" : "null");

    // Instant, non-blocking channel state check.
    const auto ch_state = channel->GetState(/*try_to_connect=*/true);
    SPDLOG_INFO("grpc_transport: channel state={} (0=IDLE 1=CONNECTING 2=READY "
                "3=TRANSIENT_FAILURE 4=SHUTDOWN)",
                static_cast<int>(ch_state));

    // Optional: prove an unary RPC call compiles by attempting a
    // heartbeat with a 1s deadline. Safe if no server is listening —
    // it fails fast and logs at DEBUG.
    HeartbeatRequest  hb;
    HeartbeatResponse resp;
    hb.set_agent_id(agent_id_);
    hb.set_agent_version("0.1.0");

    grpc::ClientContext grpc_ctx;
    grpc_ctx.set_deadline(std::chrono::system_clock::now() + 1s);

    const grpc::Status status = stub->Heartbeat(&grpc_ctx, hb, &resp);
    if (status.ok()) {
        SPDLOG_INFO("grpc_transport: heartbeat RPC ok (server_time={})",
                    resp.server_time_unix());
    } else {
        SPDLOG_DEBUG("grpc_transport: heartbeat RPC failed "
                     "(code={}, msg='{}') — expected if no gRPC server",
                     static_cast<int>(status.error_code()),
                     status.error_message());
    }

    // Idle. Real implementation will open the Connect stream here.
    while (!stop_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        std::this_thread::sleep_for(1s);
    }
}

} // namespace axiom::core

#endif // AXIOM_HAVE_GRPC
