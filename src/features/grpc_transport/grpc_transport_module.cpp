// src/features/grpc_transport/grpc_transport_module.cpp
#include "features/grpc_transport/grpc_transport_module.h"
#include "storage/state_repository.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <random>
#include <sstream>
#include <thread>

namespace axiom::core {

using namespace std::chrono_literals;
using namespace axiomdlp::agent::v1;

namespace {
constexpr const char* kStateKeyAgentId = "agent_id";

std::string to_grpc_target(std::string url, int grpc_port) {
    if (url.rfind("https://", 0) == 0) url = url.substr(8);
    else if (url.rfind("http://", 0) == 0) url = url.substr(7);
    auto slash = url.find('/');
    if (slash != std::string::npos) url = url.substr(0, slash);
    auto colon = url.find(':');
    if (colon != std::string::npos) url = url.substr(0, colon);
    return url + ":" + std::to_string(grpc_port);
}

std::string read_ca_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
} // namespace

GrpcTransportModule::GrpcTransportModule()  = default;
GrpcTransportModule::~GrpcTransportModule() = default;

bool GrpcTransportModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);
    const auto& srv = ctx_->config.server;
    if (srv.base_url.empty() || srv.api_token.empty()) return false;

    server_url_ = to_grpc_target(srv.base_url, srv.grpc_port);
    api_token_  = srv.api_token;
    SPDLOG_INFO("grpc_transport module: initialized (target={}, tls={})", server_url_, srv.grpc_tls ? "true" : "false");
    return true;
}

bool GrpcTransportModule::start() {
    stop_.store(false, std::memory_order_relaxed);
    manager_thread_ = std::thread([this] { manager_loop(); });
    running_.store(true, std::memory_order_relaxed);
    return true;
}

void GrpcTransportModule::stop() {
    if (!running_.exchange(false)) return;
    stop_.store(true, std::memory_order_relaxed);
    if (manager_thread_.joinable()) manager_thread_.join();
    SPDLOG_INFO("grpc_transport module: stopped");
}

void GrpcTransportModule::shutdown() {}

HealthStatus GrpcTransportModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("not running");
    return HealthStatus::degraded("stub — RPC implementation pending");
}

void GrpcTransportModule::manager_loop() {
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

    if (agent_id_ <= 0) return;

    std::shared_ptr<grpc::ChannelCredentials> creds;
    const auto& srv = ctx_->config.server;

    if (srv.grpc_tls) {
        grpc::SslCredentialsOptions ssl_opts;
        if (!srv.grpc_ca_path.empty()) {
            std::string ca_pem = read_ca_file(srv.grpc_ca_path);
            if (!ca_pem.empty()) ssl_opts.pem_root_certs = ca_pem;
        }
        creds = grpc::SslCredentials(ssl_opts);
    } else {
        creds = grpc::InsecureChannelCredentials();
    }

    channel_ = grpc::CreateChannel(server_url_, creds);
    stub_    = AgentService::NewStub(channel_);

    int backoff_sec = ctx_->config.server.reconnect_backoff_seconds;
    int max_backoff = ctx_->config.server.reconnect_backoff_max_seconds;
    std::mt19937 rng{std::random_device{}()};

    while (!stop_.load(std::memory_order_relaxed) && !ctx_->shutdown.is_requested()) {
        grpc::Status status = run_channel_stream();

        if (stop_.load() || ctx_->shutdown.is_requested()) break;

        if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED) {
            ctx_->shutdown.request(ShutdownReason::AuthRevoked);
            break;
        }
        if (status.error_code() == grpc::StatusCode::PERMISSION_DENIED) {
            ctx_->shutdown.request(ShutdownReason::AgentRevoked);
            break;
        }

        int jitter = std::max(1, backoff_sec / 5);
        std::uniform_int_distribution<int> dist(-jitter, jitter);
        int sleep_sec = std::max(1, backoff_sec + dist(rng));

        SPDLOG_INFO("grpc_transport: stream disconnected ({}). Reconnecting in {}s...", status.error_message(), sleep_sec);
        for (int i = 0; i < sleep_sec * 10; ++i) {
            if (stop_.load() || ctx_->shutdown.is_requested()) return;
            std::this_thread::sleep_for(100ms);
        }
        backoff_sec = std::min(backoff_sec * 2, max_backoff);
    }
}

grpc::Status GrpcTransportModule::run_channel_stream() {
    grpc::ClientContext context;
    context.AddMetadata("authorization", "Bearer " + api_token_);
    context.AddMetadata("x-agent-id", std::to_string(agent_id_));

    SPDLOG_INFO("grpc_transport: opening Channel bidi stream...");
    auto stream = stub_->Channel(&context);

    while (!stop_.load() && !ctx_->shutdown.is_requested()) {
        ChannelResponse resp;
        if (!stream->Read(&resp)) break; // Stream ended
        handle_server_message(resp);
    }

    if (stop_.load() || ctx_->shutdown.is_requested()) {
        SPDLOG_INFO("grpc_transport: sending graceful goodbye...");
        ChannelRequest req;
        auto* shut = req.mutable_shutdown();
        switch (ctx_->shutdown.reason()) {
            case ShutdownReason::UserRequested: shut->set_reason(SHUTDOWN_REASON_USER_REQUESTED); break;
            case ShutdownReason::AuthRevoked:   shut->set_reason(SHUTDOWN_REASON_AUTH_REVOKED); break;
            case ShutdownReason::AgentRevoked:  shut->set_reason(SHUTDOWN_REASON_AGENT_REVOKED); break;
            case ShutdownReason::ConfigError:   shut->set_reason(SHUTDOWN_REASON_CONFIG_ERROR); break;
            case ShutdownReason::FatalError:    shut->set_reason(SHUTDOWN_REASON_FATAL_ERROR); break;
            default:                            shut->set_reason(SHUTDOWN_REASON_UNSPECIFIED); break;
        }
        stream->Write(req);
        stream->WritesDone();
    }

    return stream->Finish();
}

void GrpcTransportModule::handle_server_message(const ChannelResponse& resp) {
    switch (resp.payload_case()) {
        case ChannelResponse::kPolicyUpdate: {
            const auto& pu = resp.policy_update();
            SPDLOG_INFO("grpc_transport: received policy_update (version={})", pu.version());
            break;
        }
        case ChannelResponse::kCommand: {
            const auto& cmd = resp.command();
            SPDLOG_INFO("grpc_transport: received command: {}", cmd.command_type());
            break;
        }
        case ChannelResponse::kRevoke: {
            const auto& rev = resp.revoke();
            SPDLOG_WARN("grpc_transport: received REVOKE (msg={})", rev.message());
            if (rev.reason() == REVOKE_REASON_TOKEN_REVOKED) ctx_->shutdown.request(ShutdownReason::AuthRevoked);
            else ctx_->shutdown.request(ShutdownReason::AgentRevoked);
            break;
        }
        default: break;
    }
}

} // namespace axiom::core