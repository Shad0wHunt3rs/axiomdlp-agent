// src/features/agent_sync/agent_sync_module.cpp
#include "features/agent_sync/agent_sync_module.h"
#include "net/http_client.h"
#include "storage/state_repository.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <fstream>
#include <sstream>
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
constexpr const char* kAgentVersion = "0.1.0";
constexpr const char* kStateKeyAgentId = "agent_id";

void interruptible_sleep(std::atomic<bool>& stop_flag, ShutdownController& shutdown, std::chrono::seconds total) {
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
    const auto& scfg = ctx_->config.server;

    enabled_                = mcfg.enabled;
    base_url_               = scfg.base_url;
    api_token_              = scfg.api_token;
    heartbeat_interval_sec_ = scfg.heartbeat_interval_seconds;

    if (!enabled_) return true;

    if (base_url_.empty() || api_token_.empty()) {
        SPDLOG_ERROR("agent_sync module: no valid API token — server.base_url or api_token is empty");
        return false;
    }

    http_ = std::make_unique<HttpClient>();
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
    http_.reset();
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
    if (agent_id_ == 0) return HealthStatus::degraded("not yet enrolled");
    return HealthStatus::ok("enrolled (agent_id=" + std::to_string(agent_id_) + ")");
}

void AgentSyncModule::sync_loop() {
    if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
        try {
            agent_id_ = std::stoi(*saved);
            SPDLOG_INFO("agent_sync: loaded agent_id={} from local state", agent_id_);
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
        SPDLOG_INFO("agent_sync: validating token with immediate heartbeat");
        if (!heartbeat()) {
            if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) return;
        }
    }

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;
        interruptible_sleep(stop_requested_, ctx_->shutdown, std::chrono::seconds(heartbeat_interval_sec_));
        if (stop_requested_.load(std::memory_order_relaxed) || ctx_->shutdown.is_requested()) break;

        if (needs_re_enroll_.exchange(false, std::memory_order_relaxed)) {
            agent_id_ = 0;
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
    const std::string url = base_url_ + "/api/v1/agent/register";
    json body = { {"hostname", detect_hostname()}, {"os", detect_os()}, {"architecture", detect_arch()}, {"agent_version", kAgentVersion} };

    SPDLOG_INFO("agent_sync: POST {} (hostname={})", url, body["hostname"].get<std::string>());
    const auto resp = http_->post_json(url, body.dump(), api_token_, 15);

    if (!resp.error.empty()) return false;
    if (resp.status == 401 || resp.status == 403) {
        ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        return false;
    }
    if (!resp.is_2xx()) return false;

    try {
        const auto parsed = json::parse(resp.body);
        agent_id_ = parsed.value("agent_id", 0);
        if (agent_id_ <= 0) return false;
        ctx_->state_store.set(kStateKeyAgentId, std::to_string(agent_id_));
        SPDLOG_INFO("agent_sync: enrolled (agent_id={}, user_id={}, org_id={})", agent_id_, parsed.value("user_id", 0), parsed.value("organization_id", 0));
        return true;
    } catch (...) { return false; }
}

bool AgentSyncModule::heartbeat() {
    if (agent_id_ == 0) {
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }

    const std::string url = base_url_ + "/api/v1/agent/heartbeat";
    json body = { {"agent_id", agent_id_}, {"agent_version", kAgentVersion} };
    const auto resp = http_->post_json(url, body.dump(), api_token_, 10);

    if (!resp.error.empty()) return false;
    if (resp.status == 200) {
        SPDLOG_DEBUG("agent_sync: heartbeat ok (agent_id={})", agent_id_);
        return true;
    }
    if (resp.status == 401 || resp.status == 403) {
        ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        return false;
    }
    if (resp.status == 404) {
        ctx_->state_store.remove(kStateKeyAgentId);
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }
    return false;
}

} // namespace axiom::core