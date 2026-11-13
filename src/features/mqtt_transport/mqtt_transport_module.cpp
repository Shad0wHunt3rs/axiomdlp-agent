// src/features/mqtt_transport/mqtt_transport_module.cpp
//
// Implements docs/mqtt_protocol.md on the agent side.
//
// Threading model:
//   * Manager thread   — owns mosq_ lifecycle, retries with backoff,
//                        publishes the shutdown frame on graceful exit.
//   * libmosquitto     — the network loop runs on its own internal
//                        thread (created by mosquitto_loop_start()).
//                        All four callbacks fire from that thread.
//   * Caller threads   — call request() / subscribe() / set_agent_id()
//                        from anywhere; the module synchronises access
//                        to mosq_ via mosq_mu_.

#include "features/mqtt_transport/mqtt_transport_module.h"
#include "events/event.h"
#include "storage/state_repository.h"

#include <mosquitto.h>
#include <mqtt_protocol.h>  // <-- Added for MQTT v5 property macros on Windows
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <utility>

namespace axiom::core {

using namespace std::chrono_literals;

namespace {

constexpr const char* kStateKeyAgentId        = "agent_id";
constexpr const char* kStateKeyMqttInstanceId = "mqtt_instance_uuid";

constexpr int kBaseBackoffSeconds   = 10;
constexpr int kMaxBackoffSeconds    = 300;
constexpr int kConnackAuthFailure   = 0x87;
constexpr int kWillDelaySeconds     = 5;

constexpr int kSessionExpirySeconds = 3600;
constexpr int kReceiveMaximum       = 100;
constexpr int kMaximumPacketSize    = 134217728;
constexpr int kKeepaliveSeconds     = 60;

std::string random_hex_32() {
    unsigned char raw[16];
    if (RAND_bytes(raw, sizeof(raw)) != 1) {
        for (std::size_t i = 0; i < sizeof(raw); ++i) {
            raw[i] = static_cast<unsigned char>(i * 31 + 7);
        }
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(32, '\0');
    for (std::size_t i = 0; i < sizeof(raw); ++i) {
        out[i * 2]     = kHex[(raw[i] >> 4) & 0xF];
        out[i * 2 + 1] = kHex[raw[i] & 0xF];
    }
    return out;
}

int compute_backoff(int attempt, int base, int max_s) {
    int64_t raw = base;
    for (int i = 0; i < attempt && raw < max_s; ++i) raw *= 2;
    if (raw > max_s) raw = max_s;
    if (raw < 1)     raw = 1;

    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.8, 1.2);
    int jittered = static_cast<int>(static_cast<double>(raw) * dist(rng));
    return jittered < 1 ? 1 : jittered;
}

} // namespace

MqttTransportModule::MqttTransportModule()  = default;
MqttTransportModule::~MqttTransportModule() { stop(); }

bool MqttTransportModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);
    mosquitto_lib_init();

    const auto& srv = ctx_->config.server;
    host_          = srv.base_url;
    api_token_     = srv.api_token;
    port_          = srv.mqtt_port;
    tls_           = srv.mqtt_tls;
    ca_path_       = srv.mqtt_ca_path;
    keepalive_sec_ = srv.mqtt_keepalive_seconds > 0 ? srv.mqtt_keepalive_seconds : kKeepaliveSeconds;
    backoff_base_  = srv.reconnect_backoff_seconds     > 0 ? srv.reconnect_backoff_seconds     : kBaseBackoffSeconds;
    backoff_max_   = srv.reconnect_backoff_max_seconds > 0 ? srv.reconnect_backoff_max_seconds : kMaxBackoffSeconds;

    if (auto p = host_.find("://"); p != std::string::npos) host_ = host_.substr(p + 3);
    if (auto p = host_.find('/');   p != std::string::npos) host_ = host_.substr(0, p);
    if (auto p = host_.find(':');   p != std::string::npos) host_ = host_.substr(0, p);

    if (host_.empty() || api_token_.empty()) {
        SPDLOG_ERROR("mqtt_transport: host or api_token empty — module disabled");
        return false;
    }

    load_or_generate_instance_uuid();
    reply_topic_ = "axiomdlp/replies/" + instance_uuid_;
    agent_id_.store(load_agent_id_from_state(), std::memory_order_relaxed);

    SPDLOG_INFO("mqtt_transport: initialized (host={}, port={}, tls={}, "
                "keepalive={}s, instance={}, agent_id={})",
                host_, port_, tls_ ? "true" : "false", keepalive_sec_,
                instance_uuid_, agent_id_.load());
    return true;
}

bool MqttTransportModule::start() {
    if (!ctx_) return false;
    stop_.store(false, std::memory_order_relaxed);
    manager_thread_ = std::thread([this] { manager_loop(); });
    running_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("mqtt_transport: started");
    return true;
}

void MqttTransportModule::stop() {
    if (!running_.exchange(false)) return;
    stop_.store(true, std::memory_order_relaxed);
    connect_cv_.notify_all();
    if (manager_thread_.joinable()) manager_thread_.join();
    SPDLOG_INFO("mqtt_transport: stopped");
}

void MqttTransportModule::shutdown() {
    mosquitto_lib_cleanup();
    SPDLOG_INFO("mqtt_transport: shutting down");
}

HealthStatus MqttTransportModule::health() const {
    if (!ctx_)              return HealthStatus::unknown("not initialized");
    if (!running_.load())   return HealthStatus::failed("not running");
    if (!connected_.load()) return HealthStatus::degraded("disconnected");
    return HealthStatus::ok("connected");
}

bool MqttTransportModule::is_connected() const {
    return connected_.load(std::memory_order_relaxed);
}

void MqttTransportModule::manager_loop() {
    int attempt = 0;

    while (!stop_.load() && !ctx_->shutdown.is_requested()) {
        if (!connect_and_setup()) {
            if (stop_.load() || ctx_->shutdown.is_requested()) return;
            int wait = compute_backoff(attempt++, backoff_base_, backoff_max_);
            SPDLOG_WARN("mqtt_transport: connect failed, retrying in {}s", wait);
            if (interruptible_sleep(wait)) return;
            continue;
        }

        attempt = 0;

        {
            std::unique_lock lock(connect_mu_);
            connect_cv_.wait(lock, [this] {
                return !connected_.load()
                    || stop_.load()
                    || ctx_->shutdown.is_requested();
            });
        }

        const bool shutting_down = stop_.load() || ctx_->shutdown.is_requested();
        if (shutting_down) publish_graceful_shutdown();
        teardown_connection();
        if (shutting_down) return;

        int wait = compute_backoff(attempt++, backoff_base_, backoff_max_);
        SPDLOG_INFO("mqtt_transport: reconnecting in {}s", wait);
        if (interruptible_sleep(wait)) return;
    }
}

bool MqttTransportModule::connect_and_setup() {
    if (stop_.load() || ctx_->shutdown.is_requested()) return false;

    connect_attempt_done_.store(false, std::memory_order_relaxed);

    const std::string client_id = "axiomdlp-" + instance_uuid_;
    mosq_ = mosquitto_new(client_id.c_str(), /*clean_start=*/false, this);
    if (!mosq_) {
        SPDLOG_ERROR("mqtt_transport: mosquitto_new failed");
        return false;
    }

    if (mosquitto_int_option(mosq_, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5)
            != MOSQ_ERR_SUCCESS) {
        SPDLOG_ERROR("mqtt_transport: failed to enable MQTT v5");
        teardown_connection();
        return false;
    }

    // Password MUST be nullptr, not "". libmosquitto treats a non-NULL
    // empty string as "password is set but empty," emitting the password
    // flag with zero length. mochi-mqtt 2.7.9 rejects that as a protocol
    // violation (CONNACK 0x82). Passing nullptr omits the password field.
    if (mosquitto_username_pw_set(mosq_, api_token_.c_str(), /*password=*/nullptr)
            != MOSQ_ERR_SUCCESS) {
        SPDLOG_ERROR("mqtt_transport: username/pw set failed");
        teardown_connection();
        return false;
    }

    if (tls_) {
        const char* ca = ca_path_.empty() ? nullptr : ca_path_.c_str();
        if (mosquitto_tls_set(mosq_, ca, nullptr, nullptr, nullptr, nullptr)
                != MOSQ_ERR_SUCCESS) {
            SPDLOG_ERROR("mqtt_transport: tls_set failed");
            teardown_connection();
            return false;
        }
        mosquitto_tls_opts_set(mosq_, SSL_VERIFY_PEER, nullptr, nullptr);
    }

    mosquitto_connect_v5_callback_set   (mosq_, &MqttTransportModule::on_connect_cb);
    mosquitto_disconnect_v5_callback_set(mosq_, &MqttTransportModule::on_disconnect_cb);
    mosquitto_message_v5_callback_set   (mosq_, &MqttTransportModule::on_message_cb);
    mosquitto_log_callback_set          (mosq_, &MqttTransportModule::on_log_cb);

    // LWT — mosquitto_will_set_v5 TAKES OWNERSHIP of will_props on success.
    {
        mosquitto_property* will_props = nullptr;
        mosquitto_property_add_int32(&will_props, MQTT_PROP_WILL_DELAY_INTERVAL,
                                     kWillDelaySeconds);

        const std::string lwt_topic   = "axiomdlp/sessions/" + instance_uuid_ + "/status";
        const std::string lwt_payload = R"({"status":"offline"})";

        const int rc = mosquitto_will_set_v5(mosq_,
                                             lwt_topic.c_str(),
                                             static_cast<int>(lwt_payload.size()),
                                             lwt_payload.data(),
                                             /*qos=*/1,
                                             /*retain=*/false,
                                             will_props);
        if (rc != MOSQ_ERR_SUCCESS) {
            mosquitto_property_free_all(&will_props);
            SPDLOG_ERROR("mqtt_transport: will_set failed: {}", mosquitto_strerror(rc));
            teardown_connection();
            return false;
        }
    }

    mosquitto_property* connect_props = nullptr;
    mosquitto_property_add_int32(&connect_props, MQTT_PROP_SESSION_EXPIRY_INTERVAL,
                                 kSessionExpirySeconds);
    mosquitto_property_add_int16(&connect_props, MQTT_PROP_RECEIVE_MAXIMUM,
                                 kReceiveMaximum);
    mosquitto_property_add_int32(&connect_props, MQTT_PROP_MAXIMUM_PACKET_SIZE,
                                 kMaximumPacketSize);
    mosquitto_property_add_int16(&connect_props, MQTT_PROP_TOPIC_ALIAS_MAXIMUM,
                                 0);

    errno = 0;
    int rc = mosquitto_connect_bind_v5(mosq_,
                                       host_.c_str(),
                                       port_,
                                       keepalive_sec_,
                                       /*bind_address=*/nullptr,
                                       connect_props);
    const int saved_errno = errno;

    mosquitto_property_free_all(&connect_props);

    if (rc != MOSQ_ERR_SUCCESS) {
        if (!ctx_->shutdown.is_requested()) {
            errno = saved_errno;
            SPDLOG_ERROR("mqtt_transport: connect_bind_v5 failed: {} "
                         "(host={} port={} errno={})",
                         mosquitto_strerror(rc), host_, port_, saved_errno);
        }
        teardown_connection();
        return false;
    }

    rc = mosquitto_loop_start(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        SPDLOG_ERROR("mqtt_transport: loop_start failed: {}", mosquitto_strerror(rc));
        teardown_connection();
        return false;
    }

    if (!connected_.load()) {
        std::unique_lock lock(connect_mu_);
        connect_cv_.wait_for(lock, 5s,
                             [this] { return connect_attempt_done_.load(); });
    }

    if (!connected_.load()) {
        SPDLOG_WARN("mqtt_transport: connect callback did not fire");
        teardown_connection();
        return false;
    }

    // Subscribe from the manager thread (not from handle_connect on the
    // loop thread) — this is what prevents a deadlock against
    // mosquitto_loop_stop in teardown_connection, which waits for the
    // loop thread to exit before destroying the client.
    subscribe_connected_topics();

    return true;
}

void MqttTransportModule::teardown_connection() {
    std::lock_guard<std::mutex> lock(mosq_mu_);
    if (!mosq_) {
        connected_.store(false, std::memory_order_relaxed);
        return;
    }
    mosquitto_disconnect_v5(mosq_, 0, nullptr);
    mosquitto_loop_stop(mosq_, /*force=*/false);
    mosquitto_destroy(mosq_);
    mosq_ = nullptr;
    connected_.store(false, std::memory_order_relaxed);
    connect_attempt_done_.store(false, std::memory_order_relaxed);
}

void MqttTransportModule::publish_graceful_shutdown() {
    const int aid = agent_id_.load(std::memory_order_relaxed);
    if (aid <= 0) return;

    const nlohmann::json msg = {
        {"v",        1},
        {"agent_id", aid},
        {"reason",   axiom::core::to_string(ctx_->shutdown.reason())},
    };
    const std::string body  = msg.dump();
    const std::string topic = "axiomdlp/agents/" + std::to_string(aid) + "/shutdown";

    std::lock_guard<std::mutex> lock(mosq_mu_);
    if (!mosq_ || !connected_.load()) return;

    int mid = 0;
    const int rc = mosquitto_publish_v5(mosq_, &mid,
                                        topic.c_str(),
                                        static_cast<int>(body.size()), body.data(),
                                        /*qos=*/1, /*retain=*/false,
                                        /*props=*/nullptr);
    if (rc != MOSQ_ERR_SUCCESS) {
        SPDLOG_WARN("mqtt_transport: shutdown publish failed: {}", mosquitto_strerror(rc));
    } else {
        SPDLOG_INFO("mqtt_transport: published shutdown message");
    }
}

bool MqttTransportModule::interruptible_sleep(int seconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    constexpr auto tick = 100ms;
    while (std::chrono::steady_clock::now() < deadline) {
        if (stop_.load() || ctx_->shutdown.is_requested()) return true;
        std::this_thread::sleep_for(tick);
    }
    return stop_.load() || ctx_->shutdown.is_requested();
}

// ── Callbacks (fire on the mosquitto loop thread) ─────────────────

void MqttTransportModule::on_connect_cb(struct mosquitto* /*mosq*/,
                                        void* obj, int rc, int flags,
                                        const mosquitto_property* props) {
    static_cast<MqttTransportModule*>(obj)->handle_connect(rc, flags, props);
}

void MqttTransportModule::on_disconnect_cb(struct mosquitto* /*mosq*/,
                                           void* obj, int rc,
                                           const mosquitto_property* /*props*/) {
    static_cast<MqttTransportModule*>(obj)->handle_disconnect(rc);
}

void MqttTransportModule::on_message_cb(struct mosquitto* /*mosq*/,
                                        void* obj,
                                        const struct mosquitto_message* msg,
                                        const mosquitto_property* props) {
    static_cast<MqttTransportModule*>(obj)->handle_message(msg, props);
}

void MqttTransportModule::on_log_cb(struct mosquitto* /*mosq*/,
                                    void* /*obj*/, int level, const char* str) {
    if (str) SPDLOG_DEBUG("[mqtt/{}] {}", level, str);
}

void MqttTransportModule::handle_connect(int rc, int flags,
                                         const mosquitto_property* /*props*/) {
    (void)flags;

    if (rc != 0) {
        SPDLOG_ERROR("mqtt_transport: CONNACK rejected (reason=0x{:02x} {})",
                     rc, mosquitto_connack_string(rc));
        if (rc == kConnackAuthFailure) {
            ctx_->shutdown.request(ShutdownReason::AuthRevoked);
        }
        connect_attempt_done_.store(true, std::memory_order_relaxed);
        connect_cv_.notify_all();
        return;
    }

    connected_.store(true, std::memory_order_relaxed);
    connect_attempt_done_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("mqtt_transport: connected to {}:{}", host_, port_);

    {
        auto e = std::make_shared<Event>(Event::create(event_types::kMqttState));
        e->metadata["state"]  = "connected";
        e->metadata["host"]   = host_;
        e->metadata["source"] = "mqtt_transport";
        ctx_->event_bus.publish(e);
    }

    // Subscribe happens in connect_and_setup() after the CV wait —
    // do NOT subscribe from this callback (see that function's comment).
    connect_cv_.notify_all();
}

void MqttTransportModule::handle_disconnect(int rc) {
    const bool was_connected = connected_.exchange(false, std::memory_order_relaxed);

    if (was_connected) {
        SPDLOG_WARN("mqtt_transport: disconnected (reason=0x{:02x} {})",
                    rc, mosquitto_connack_string(rc));
    } else {
        connect_attempt_done_.store(true, std::memory_order_relaxed);
    }

    {
        auto e = std::make_shared<Event>(Event::create(event_types::kMqttState));
        e->metadata["state"]  = "disconnected";
        e->metadata["reason"] = mosquitto_connack_string(rc);
        e->metadata["source"] = "mqtt_transport";
        ctx_->event_bus.publish(e);
    }

    connect_cv_.notify_all();
}

void MqttTransportModule::handle_message(const struct mosquitto_message* msg,
                                         const mosquitto_property* props) {
    if (!msg || !msg->topic) return;

    std::string correlation_id;
    if (props) {
        void*    data = nullptr;
        uint16_t len  = 0;
        if (mosquitto_property_read_binary(
                props, MQTT_PROP_CORRELATION_DATA,
                &data, &len, /*skip_first=*/false) != nullptr
            && data != nullptr && len > 0) {
            correlation_id.assign(static_cast<const char*>(data),
                                  static_cast<std::size_t>(len));
            std::free(data);
        }
    }

    nlohmann::json body;
    try {
        if (msg->payloadlen > 0) {
            const char* begin = static_cast<const char*>(msg->payload);
            const char* end   = begin + msg->payloadlen;
            body = nlohmann::json::parse(begin, end);
        }
    } catch (const std::exception& e) {
        SPDLOG_WARN("mqtt_transport: bad JSON on '{}': {}", msg->topic, e.what());
        return;
    }

    if (correlation_id.empty()
        && body.is_object()
        && body.contains("correlation_id")
        && body["correlation_id"].is_string()) {
        correlation_id = body["correlation_id"].get<std::string>();
    }

    if (!correlation_id.empty()) {
        std::shared_ptr<PendingRequest> pending;
        {
            std::lock_guard lock(pending_mu_);
            auto it = pending_.find(correlation_id);
            if (it != pending_.end()) pending = it->second;
        }
        if (pending) {
            {
                std::lock_guard lk(pending->mu);
                pending->response = std::move(body);
                pending->done     = true;
            }
            pending->cv.notify_all();
            return;
        }
    }

    if (msg->topic && std::string(msg->topic).find("/revoke") != std::string::npos) {
        handle_server_revoke(body);
        return;
    }

    std::vector<MessageHandler> handlers;
    {
        std::lock_guard lock(subs_mu_);
        auto it = subs_.find(msg->topic);
        if (it != subs_.end()) handlers = it->second;
    }

    if (handlers.empty()) {
        SPDLOG_DEBUG("mqtt_transport: no handler for topic '{}'", msg->topic);
        return;
    }

    for (const auto& h : handlers) {
        try { h(body); }
        catch (const std::exception& e) {
            SPDLOG_ERROR("mqtt_transport: handler threw on '{}': {}",
                         msg->topic, e.what());
        }
    }
}

void MqttTransportModule::handle_server_revoke(const nlohmann::json& body) {
    const std::string reason =
        body.is_object() && body.contains("reason") && body["reason"].is_string()
            ? body["reason"].get<std::string>()
            : "";
    SPDLOG_WARN("mqtt_transport: server REVOKE (reason='{}')", reason);

    if (reason == "token_revoked")      ctx_->shutdown.request(ShutdownReason::AuthRevoked);
    else if (reason == "agent_revoked") ctx_->shutdown.request(ShutdownReason::AgentRevoked);
    else                                ctx_->shutdown.request(ShutdownReason::AuthRevoked);
}

void MqttTransportModule::subscribe_connected_topics() {
    std::lock_guard<std::mutex> lock(mosq_mu_);
    if (!mosq_) return;

    auto do_sub = [this](const std::string& topic, int qos) {
        int mid = 0;
        const int rc = mosquitto_subscribe(mosq_, &mid, topic.c_str(), qos);
        if (rc != MOSQ_ERR_SUCCESS) {
            SPDLOG_WARN("mqtt_transport: subscribe '{}' failed: {}",
                        topic, mosquitto_strerror(rc));
        } else {
            SPDLOG_INFO("mqtt_transport: subscribed '{}' (qos={})", topic, qos);
        }
    };

    do_sub(reply_topic_, 1);

    const int aid = agent_id_.load(std::memory_order_relaxed);
    if (aid > 0) {
        do_sub("axiomdlp/agents/" + std::to_string(aid) + "/command", 2);
        do_sub("axiomdlp/agents/" + std::to_string(aid) + "/policy",  2);
        do_sub("axiomdlp/agents/" + std::to_string(aid) + "/revoke",  2);
    }
}

void MqttTransportModule::set_agent_id(int agent_id) {
    if (agent_id < 0) return;   // negative invalid; 0 = clear (valid)

    const int prev = agent_id_.exchange(agent_id, std::memory_order_relaxed);
    if (prev == agent_id) return;

    SPDLOG_INFO("mqtt_transport: agent_id set to {} (was {})", agent_id, prev);

    if (connected_.load(std::memory_order_relaxed)) {
        subscribe_connected_topics();
    }
}

// ── IRequestClient ────────────────────────────────────────────────

std::optional<nlohmann::json>
MqttTransportModule::request(const std::string&    topic,
                             const nlohmann::json& payload,
                             int                   qos,
                             int                   timeout_seconds) {
    if (!connected_.load(std::memory_order_relaxed)) {
        SPDLOG_DEBUG("mqtt_transport: request on disconnected client (topic={})", topic);
        return std::nullopt;
    }

    const std::string cid = random_hex_32();
    nlohmann::json body   = payload;
    body["v"]              = 1;
    body["correlation_id"] = cid;
    const std::string body_str = body.dump();

    auto pending = std::make_shared<PendingRequest>();
    {
        std::lock_guard lock(pending_mu_);
        pending_[cid] = pending;
    }

    mosquitto_property* props = nullptr;
    mosquitto_property_add_string(&props, MQTT_PROP_RESPONSE_TOPIC,
                                  reply_topic_.c_str());
    mosquitto_property_add_binary(&props, MQTT_PROP_CORRELATION_DATA,
                                  cid.data(),
                                  static_cast<uint16_t>(cid.size()));

    int mid = 0;
    int rc  = MOSQ_ERR_SUCCESS;
    {
        std::lock_guard<std::mutex> lock(mosq_mu_);
        if (!mosq_) {
            mosquitto_property_free_all(&props);
            std::lock_guard plk(pending_mu_);
            pending_.erase(cid);
            return std::nullopt;
        }
        rc = mosquitto_publish_v5(mosq_, &mid,
                                  topic.c_str(),
                                  static_cast<int>(body_str.size()),
                                  body_str.data(),
                                  qos, /*retain=*/false, props);
    }
    mosquitto_property_free_all(&props);

    if (rc != MOSQ_ERR_SUCCESS) {
        {
            std::lock_guard lock(pending_mu_);
            pending_.erase(cid);
        }
        SPDLOG_WARN("mqtt_transport: publish '{}' failed: {}",
                    topic, mosquitto_strerror(rc));
        return std::nullopt;
    }

    std::unique_lock lock(pending->mu);
    const bool done = pending->cv.wait_for(
        lock, std::chrono::seconds(timeout_seconds),
        [&] { return pending->done; });

    {
        std::lock_guard lk(pending_mu_);
        pending_.erase(cid);
    }

    if (!done) {
        SPDLOG_DEBUG("mqtt_transport: request '{}' timed out after {}s",
                     topic, timeout_seconds);
        return std::nullopt;
    }
    return pending->response;
}

void MqttTransportModule::subscribe(
        const std::string& topic,
        std::function<void(const nlohmann::json&)> handler) {
    std::lock_guard lock(subs_mu_);
    subs_[topic].push_back(std::move(handler));
}

bool MqttTransportModule::publish(const std::string&    topic,
                                  const nlohmann::json& payload,
                                  int                   qos,
                                  bool                  retain) {
    if (!connected_.load(std::memory_order_relaxed)) return false;

    nlohmann::json body = payload;
    if (!body.is_object()) return false;
    if (!body.contains("v")) body["v"] = 1;
    const std::string s = body.dump();

    std::lock_guard<std::mutex> lock(mosq_mu_);
    if (!mosq_) return false;

    int mid = 0;
    const int rc = mosquitto_publish_v5(mosq_, &mid,
                                        topic.c_str(),
                                        static_cast<int>(s.size()), s.data(),
                                        qos, retain, /*props=*/nullptr);
    return rc == MOSQ_ERR_SUCCESS;
}

// ── State helpers ─────────────────────────────────────────────────

void MqttTransportModule::load_or_generate_instance_uuid() {
    if (auto existing = ctx_->state_store.get(kStateKeyMqttInstanceId)) {
        if (existing->size() == 32) {
            instance_uuid_ = *existing;
            return;
        }
        SPDLOG_WARN("mqtt_transport: stored mqtt_instance_uuid has unexpected "
                    "length {} — regenerating", existing->size());
    }
    instance_uuid_ = random_hex_32();
    ctx_->state_store.set(kStateKeyMqttInstanceId, instance_uuid_);
    SPDLOG_INFO("mqtt_transport: generated new instance_uuid={}", instance_uuid_);
}

int MqttTransportModule::load_agent_id_from_state() const {
    if (auto v = ctx_->state_store.get(kStateKeyAgentId)) {
        try {
            const int id = std::stoi(*v);
            if (id > 0) return id;
        } catch (const std::exception&) { /* ignore */ }
    }
    return 0;
}

} // namespace axiom::core