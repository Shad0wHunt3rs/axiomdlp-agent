// src/features/agent_sync/agent_sync_module.cpp

#include "features/agent_sync/agent_sync_module.h"
#include "net/http_client.h"
#include "storage/state_repository.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <boost/asio/ip/host_name.hpp>

#include <chrono>
#include <string>
#include <thread>

namespace axiom::core {

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

#ifndef AXIOM_AGENT_VERSION
#  define AXIOM_AGENT_VERSION "0.1.0"
#endif
constexpr const char* kAgentVersion = AXIOM_AGENT_VERSION;

constexpr const char* kStateKeyAgentId = "agent_id";

void interruptible_sleep(std::atomic<bool>& stop_flag,
                         ShutdownController& shutdown,
                         std::chrono::seconds total) {
    constexpr auto tick = 100ms;
    auto elapsed = 0ms;
    const auto target =
        std::chrono::duration_cast<std::chrono::milliseconds>(total);

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
    try {
        return boost::asio::ip::host_name();
    } catch (...) {
        return "unknown";
    }
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

bool AgentSyncModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    const auto& mcfg = ctx_->config.modules.agent_sync;
    const auto& scfg = ctx_->config.server;

    enabled_                = mcfg.enabled;
    http_base_url_          = scfg.base_url;
    api_token_              = scfg.api_token;
    heartbeat_interval_sec_ = scfg.heartbeat_interval_seconds;

    if (!enabled_) {
        SPDLOG_INFO("agent_sync module: disabled in config");
        return true;
    }

    if (http_base_url_.empty()) {
        SPDLOG_ERROR("agent_sync module: server.base_url is empty");
        return false;
    }

    if (api_token_.empty()) {
        SPDLOG_ERROR("agent_sync module: no valid API token — "
                     "server.api_token is empty. "
                     "Set server.api_token in the config file.");
        return false;
    }

    SPDLOG_INFO("agent_sync module: initialized "
                "(server={}, heartbeat_interval={}s)",
                http_base_url_, heartbeat_interval_sec_);
    return true;
}

bool AgentSyncModule::start() {
    if (!enabled_) return true;

    http_ = std::make_unique<HttpClient>();

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

    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }

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
    if (!running_.load(std::memory_order_relaxed)) {
        return HealthStatus::failed("sync thread not running");
    }
    if (agent_id_ == 0) {
        return HealthStatus::degraded("not yet enrolled");
    }
    return HealthStatus::ok("enrolled (agent_id=" + std::to_string(agent_id_) + ")");
}

void AgentSyncModule::sync_loop() {
    if (auto saved = ctx_->state_store.get(kStateKeyAgentId)) {
        try {
            agent_id_ = std::stoi(*saved);
            SPDLOG_INFO("agent_sync: loaded agent_id={} from local state",
                        agent_id_);
        } catch (const std::exception&) {
            SPDLOG_WARN("agent_sync: invalid saved agent_id '{}'; will re-enroll",
                        *saved);
            ctx_->state_store.remove(kStateKeyAgentId);
            agent_id_ = 0;
        }
    }

    if (agent_id_ == 0) {
        if (!enroll()) {
            if (stop_requested_.load(std::memory_order_relaxed)) return;
            if (ctx_->shutdown.is_requested()) return;
            SPDLOG_ERROR("agent_sync: initial enrollment failed; will retry");
            needs_re_enroll_.store(true, std::memory_order_relaxed);
        }
    } else {
        SPDLOG_INFO("agent_sync: validating token with immediate heartbeat");
        if (!heartbeat()) {
            if (stop_requested_.load(std::memory_order_relaxed)) return;
            if (ctx_->shutdown.is_requested()) return;
            SPDLOG_WARN("agent_sync: initial heartbeat failed; "
                        "will retry on next cycle");
        }
    }

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        if (ctx_->shutdown.is_requested()) return;

        interruptible_sleep(stop_requested_, ctx_->shutdown,
                            std::chrono::seconds(heartbeat_interval_sec_));

        if (stop_requested_.load(std::memory_order_relaxed)) break;
        if (ctx_->shutdown.is_requested()) break;

        if (needs_re_enroll_.exchange(false, std::memory_order_relaxed)) {
            agent_id_ = 0;
            if (!enroll()) {
                if (stop_requested_.load(std::memory_order_relaxed)) return;
                if (ctx_->shutdown.is_requested()) return;
                SPDLOG_WARN("agent_sync: re-enrollment failed; "
                            "will retry next cycle");
                needs_re_enroll_.store(true, std::memory_order_relaxed);
                continue;
            }
        }

        if (!heartbeat()) {
            if (stop_requested_.load(std::memory_order_relaxed)) return;
            if (ctx_->shutdown.is_requested()) return;
        }
    }
}

bool AgentSyncModule::enroll() {
    const std::string url = http_base_url_ + "/api/v1/agent/register";

    json body = {
        {"hostname",      detect_hostname()},
        {"os",            detect_os()},
        {"agent_version", kAgentVersion}
    };

    SPDLOG_INFO("agent_sync: POST {} (hostname={})",
                url, body["hostname"].get<std::string>());

    const auto resp = http_->post_json(url, body.dump(), api_token_, 15);

    if (!resp.error.empty()) {
        SPDLOG_WARN("agent_sync: enrollment transport error: {}", resp.error);
        return false;
    }

    if (resp.status == 401 || resp.status == 403) {
        SPDLOG_ERROR("agent_sync: no valid API token — "
                     "enrollment rejected ({}) — "
                     "token is invalid, expired, or revoked",
                     resp.status);
        ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        return false;
    }

    if (!resp.is_2xx()) {
        SPDLOG_WARN("agent_sync: enrollment failed with HTTP {}: {}",
                    resp.status, resp.body);
        return false;
    }

    try {
        const auto parsed = json::parse(resp.body);
        agent_id_ = parsed.value("agent_id", 0);
        const int user_id = parsed.value("user_id", 0);
        const int org_id  = parsed.value("organization_id", 0);

        if (agent_id_ <= 0) {
            SPDLOG_ERROR("agent_sync: server returned invalid agent_id: {}",
                         resp.body);
            return false;
        }

        ctx_->state_store.set(kStateKeyAgentId, std::to_string(agent_id_));

        SPDLOG_INFO("agent_sync: enrolled (agent_id={}, user_id={}, org_id={})",
                    agent_id_, user_id, org_id);
        return true;

    } catch (const std::exception& e) {
        SPDLOG_ERROR("agent_sync: failed to parse enrollment response: {} (body: {})",
                     e.what(), resp.body);
        return false;
    }
}

bool AgentSyncModule::heartbeat() {
    if (agent_id_ == 0) {
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }

    const std::string url = http_base_url_ + "/api/v1/agent/heartbeat";

    json body = {
        {"agent_id",      agent_id_},
        {"agent_version", kAgentVersion}
    };

    const auto resp = http_->post_json(url, body.dump(), api_token_, 10);

    if (!resp.error.empty()) {
        SPDLOG_WARN("agent_sync: heartbeat transport error: {}", resp.error);
        return false;
    }

    if (resp.status == 200) {
        SPDLOG_DEBUG("agent_sync: heartbeat ok (agent_id={})", agent_id_);
        return true;
    }

    if (resp.status == 401) {
        SPDLOG_ERROR("agent_sync: no valid API token — "
                     "heartbeat rejected (401) — "
                     "token is invalid, expired, or revoked");
        ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        return false;
    }

    if (resp.status == 403) {
        SPDLOG_ERROR("agent_sync: no valid API token — "
                     "heartbeat rejected (403) — "
                     "this agent has been revoked server-side");
        ctx_->shutdown.request(ShutdownReason::AgentRevoked);
        return false;
    }

    if (resp.status == 404) {
        SPDLOG_WARN("agent_sync: heartbeat returned 404 — "
                    "agent_id no longer exists server-side; re-enrolling");
        ctx_->state_store.remove(kStateKeyAgentId);
        needs_re_enroll_.store(true, std::memory_order_relaxed);
        return false;
    }

    SPDLOG_WARN("agent_sync: heartbeat returned HTTP {}: {}",
                resp.status, resp.body);
    return false;
}

} // namespace axiom::core