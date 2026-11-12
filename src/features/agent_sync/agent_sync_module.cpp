// src/features/agent_sync/agent_sync_module.cpp
//
// Phase 8: enrollment and heartbeat run over MQTT (IRequestClient::request)
// against the topics defined in docs/mqtt_protocol.md §9.1 and §9.2.
//
// The response envelope is {"ok": bool, "error"?: string, ...payload}.
// On successful register we persist agent_id and call set_agent_id() on
// the transport so it subscribes to the per-agent topics.

#include "features/agent_sync/agent_sync_module.h"
#include "storage/state_repository.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <string>
#include <thread>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__linux__)
#  include <unistd.h>
#endif

namespace axiom::core {

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

constexpr const char* kAgentVersion   = "0.1.0";
constexpr const char* kStateKeyAgentId = "agent_id";

void interruptible_sleep(std::atomic<bool>& stop_flag,
                         ShutdownController& shutdown,
                         std::chrono::seconds total) {
    constexpr auto tick = 100ms;
    auto elapsed = 0ms;
    const auto target = std::chrono::duration_cast<std::chrono::milliseconds>(total);
    while (elapsed < target) {
        if (stop_flag.load(std::memory_order_relaxed)) return;
        if (shutdown.is_requested()) return;
        std::this_thread::sleep_for(tick);
        elapsed += tick;
    }
}

} // namespace

AgentSyncModule::AgentSyncModule()  = default;
AgentSyncModule::~AgentSyncModule() = default;

std::string AgentSyncModule::detect_hostname() {
#if defined(_WIN32)
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) return std::string(buf, size);
#elif defined(__linux__)
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0) return std::string(buf);
#endif
    return "unknown";
}

std::string AgentSyncModule::detect_os() {
#if defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "darwin";
#else
    return "unknown";
#endif
}

std::string AgentSyncModule::detect_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "i686";
#elif defined(__aarch64__)
    return "aarch64";
#else
    return "unknown";
#endif
}

bool AgentSyncModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    const auto& mcfg = ctx_->config.modules.agent_sync;

    enabled_ = mcfg.enabled;
    heartbeat_interval_sec_ = ctx_->config.server.heartbeat_interval_seconds;
    if (heartbeat_interval_sec_ < 5) heartbeat_interval_sec_ = 60;

    if (!enabled_) return true;

    // The request client is provided by the transport module through
    // ModuleContext. If MQTT is disabled at build time it is null; the
    // module then stays idle instead of failing the whole app.
    if (ctx_->request_client == nullptr) {
        SPDLOG_ERROR("agent_sync module: no request client available "
                     "(MQTT disabled?); module will remain idle");
        enabled_ = false;
        return true;
    }
    req_ = ctx_->request_client;

    SPDLOG_INFO("agent_sync module: initialized (heartbeat={}s)", heartbeat_interval_sec_);
    return true;
}

bool AgentSyncModule::start() {
    if (!enabled_) return true;
    stop_requested_.store(false, std::memory_order_relaxed);
    needs_re_enroll_.store(false, std::memory_order_relaxed);
    sync_thread_ = std::thread([this] { sync_loop(); });
    running_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("agent_sync module: started");
    return true;
}

void AgentSyncModule::stop() {
    if (!running_.load(std::memory_order_relaxed)) return;
    stop_requested_.store(true, std::memory_order_relaxed);
    if (sync_thread_.joinable()) sync_thread_.join();
    running_.store(false, std::memory_order_relaxed);
    SPDLOG_INFO("agent_sync module: stopped");
}

void AgentSyncModule::shutdown() {
    SPDLOG_INFO("agent_sync module: shutting down");
}

HealthStatus AgentSyncModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!enabled_)       return HealthStatus::ok("disabled");
    if (!running_.load(std::memory_order_relaxed)) return HealthStatus::failed("sync thread not running");
    if (agent_id_ == 0)  return HealthStatus::degraded("not yet enrolled");
    return HealthStatus::ok("enrolled (agent_id=" + std::to_string(agent_id_) + ")");
}

void AgentSyncModule::wait_for_transport_ready() {
    // The transport starts before us in registration order, but its
    // connect is asynchronous. Give it a bounded window to come up so
    // the very first register attempt has a chance to succeed.
    constexpr int kReadyTimeoutSec = 30;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kReadyTimeoutSec);

    while (std::chrono::steady_clock::now() < deadline) {
        if (stop_requested_.load(std::memory_order_relaxed)) return;
        if (ctx_->shutdown.is_requested()) return;
        if (req_->is_connected()) return;
        std::this_thread::sleep_for(200ms);
    }
    SPDLOG_WARN("agent_sync: transport not connected after {}s; "
                "registration will be retried", kReadyTimeoutSec);
}

void AgentSyncModule::sync_loop() {
    wait_for_transport_ready();
    if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;

    // Load agent_id from local state; if present, tell the transport
    // immediately so its subscriptions include the per-agent topics.
    if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
        try {
            agent_id_ = std::stoi(*saved);
            SPDLOG_INFO("agent_sync: loaded agent_id={} from local state", agent_id_);
            req_->set_agent_id(agent_id_);
        } catch (const std::exception&) {
            ctx_->state_store.remove(kStateKeyAgentId);
            agent_id_ = 0;
        }
    }

    if (agent_id_ == 0) {
        if (!enroll()) {
            if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
            needs_re_enroll_.store(true, std::memory_order_relaxed);
        }
    } else {
        SPDLOG_INFO("agent_sync: validating saved agent_id={}", agent_id_);
        if (!heartbeat()) {
            if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
        }
    }

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;

        // Pending re-enrolls retry sooner than the steady-state interval.
        const auto wait_secs = needs_re_enroll_.load(std::memory_order_relaxed)
            ? std::chrono::seconds(5)
            : std::chrono::seconds(heartbeat_interval_sec_);
        interruptible_sleep(stop_requested_, ctx_->shutdown, wait_secs);

        if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) break;

        if (needs_re_enroll_.exchange(false, std::memory_order_relaxed)) {
            agent_id_ = 0;
            req_->set_agent_id(0);
            if (!enroll()) {
                if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
                needs_re_enroll_.store(true, std::memory_order_relaxed);
                continue;
            }
        }

        if (!heartbeat()) {
            if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
        }
    }
}

bool AgentSyncModule::enroll() {
    json body = {
        {"hostname",      detect_hostname()},
        {"os",            detect_os()},
        {"architecture",  detect_arch()},
        {"agent_version", kAgentVersion},
    };

    SPDLOG_INFO("agent_sync: registering over MQTT (hostname={})",
                body["hostname"].get<std::string>());

    const auto resp = req_->request("axiomdlp/register", body,
                                    /*qos=*/1, /*timeout_sec=*/15);

    if (!resp) {
        SPDLOG_WARN("agent_sync: register request failed or timed out");
        return false;
    }

    if (!resp->value("ok", false)) {
        const std::string err = resp->value("error", "");
        SPDLOG_WARN("agent_sync: register rejected: error='{}'", err);

        if (err == "agent_revoked") {
            ctx_->shutdown.request(ShutdownReason::AgentRevoked);
        }
        return false;
    }

    const int agent_id = resp->value("agent_id", 0);
    if (agent_id <= 0) {
        SPDLOG_WARN("agent_sync: register response missing agent_id");
        return false;
    }

    agent_id_ = agent_id;
    ctx_->state_store.set(kStateKeyAgentId, std::to_string(agent_id_));

    // Tell the transport so it subscribes to the per-agent topics.
    req_->set_agent_id(agent_id_);

    SPDLOG_INFO("agent_sync: enrolled (agent_id={}, user_id={}, org_id={})",
                agent_id_,
                resp->value("user_id", 0),
                resp->value("organization_id", 0));
    return true;
}

bool AgentSyncModule::heartbeat() {
    if (agent_id_ == 0) {
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }

    json body = {
        {"agent_id",      agent_id_},
        {"agent_version", kAgentVersion},
    };

    const std::string topic =
        "axiomdlp/agents/" + std::to_string(agent_id_) + "/heartbeat";

    const auto resp = req_->request(topic, body, /*qos=*/1, /*timeout_sec=*/10);

    if (!resp) {
        SPDLOG_DEBUG("agent_sync: heartbeat request failed or timed out");
        return false;
    }

    if (resp->value("ok", false)) {
        SPDLOG_DEBUG("agent_sync: heartbeat ok (agent_id={})", agent_id_);
        return true;
    }

    const std::string err = resp->value("error", "");
    if (err == "agent_revoked") {
        SPDLOG_WARN("agent_sync: agent revoked by server");
        ctx_->shutdown.request(ShutdownReason::AgentRevoked);
        return false;
    }
    if (err == "not_found") {
        SPDLOG_WARN("agent_sync: agent no longer known to server; will re-enroll");
        ctx_->state_store.remove(kStateKeyAgentId);
        agent_id_ = 0;
        req_->set_agent_id(0);
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }

    SPDLOG_WARN("agent_sync: heartbeat error: '{}'", err);
    return false;
}

} // namespace axiom::core