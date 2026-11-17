// src/features/mqtt_transport/mqtt_transport_module.h
//
// MqttTransportModule is the sole server-facing transport for the agent.
// It owns one libmosquitto client, one manager thread, and the correlated
// request/response machinery described in docs/mqtt_protocol.md.
//
// Forward declarations of libmosquitto types MUST match libmosquitto's own
// header exactly.

#pragma once

#include "modules/module_base.h"
#include "net/i_request_client.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

// ----------------------------------------------------------------
// Forward declarations — must match libmosquitto's own headers EXACTLY.
// ----------------------------------------------------------------
struct mosquitto;
struct mosquitto_message;
struct mqtt5__property;
typedef struct mqtt5__property mosquitto_property;

namespace axiom::core {

class MqttTransportModule final : public ModuleBase,
                                  public IRequestClient {
public:
    using MessageHandler = std::function<void(const nlohmann::json&)>;

    MqttTransportModule();
    ~MqttTransportModule() override;

    std::string name() const override { return "mqtt_transport"; }

    // ── Module lifecycle ───────────────────────────────────────────
    bool         initialize(ModuleContext& context) override;
    bool         start() override;
    void         stop() override;
    void         shutdown() override;
    HealthStatus health() const override;

    // ── IRequestClient ─────────────────────────────────────────────
    std::optional<nlohmann::json> request(
        const std::string&    topic,
        const nlohmann::json& payload,
        int                   qos,
        int                   timeout_seconds) override;

    void subscribe(const std::string& topic,
                   std::function<void(const nlohmann::json&)> handler) override;

    void unsubscribe(const std::string& topic) override;

    bool is_connected() const override;

    void set_agent_id(int agent_id) override;

    // ── MQTT-specific extension ────────────────────────────────────
    IRequestClient* as_request_client() { return this; }

    bool publish(const std::string&    topic,
                 const nlohmann::json& payload,
                 int                   qos,
                 bool                  retain);

private:
    struct PendingRequest {
        std::mutex              mu;
        std::condition_variable cv;
        nlohmann::json          response;
        bool                    done = false;
    };

    void manager_loop();
    bool connect_and_setup();
    void teardown_connection();
    void subscribe_connected_topics();
    bool interruptible_sleep(int seconds);
    void publish_graceful_shutdown();

    static void on_connect_cb(struct mosquitto* mosq, void* obj,
                              int rc, int flags,
                              const mosquitto_property* props);
    static void on_disconnect_cb(struct mosquitto* mosq, void* obj,
                                 int rc, const mosquitto_property* props);
    static void on_message_cb(struct mosquitto* mosq, void* obj,
                              const struct mosquitto_message* msg,
                              const mosquitto_property* props);
    static void on_log_cb(struct mosquitto* mosq, void* obj,
                          int level, const char* str);

    void handle_connect(int rc, int flags, const mosquitto_property* props);
    void handle_disconnect(int rc);
    void handle_message(const struct mosquitto_message* msg,
                        const mosquitto_property* props);
    void handle_server_revoke(const nlohmann::json& body);

    void load_or_generate_instance_uuid();
    int  load_agent_id_from_state() const;

    std::string host_;
    int         port_          = 8883;
    bool        tls_           = true;
    std::string ca_path_;
    int         keepalive_sec_ = 60;
    int         backoff_base_  = 10;
    int         backoff_max_   = 300;
    std::string api_token_;

    std::string      instance_uuid_;
    std::string      reply_topic_;
    std::atomic<int> agent_id_{0};

    // BUG-B1: track which agent_id's per-agent topics the broker
    // believes we are subscribed to. Persists across connection teardown
    // so the diff check in subscribe_connected_topics can drop stale
    // {prev_id}/{command,policy,revoke} subscriptions even when the
    // agent_id changed while we were disconnected (Clean Start is false
    // so the broker may retain them across the blip).
    std::atomic<int> last_subscribed_agent_id_{0};

    struct mosquitto* mosq_ = nullptr;
    mutable std::mutex mosq_mu_;    // guards mosq_ against cross-thread use

    std::atomic<bool>       stop_{false};
    std::atomic<bool>       running_{false};
    std::atomic<bool>       connected_{false};
    std::atomic<bool>       connect_attempt_done_{false};
    std::thread             manager_thread_;
    mutable std::mutex      connect_mu_;
    std::condition_variable connect_cv_;

    mutable std::mutex                                subs_mu_;
    std::map<std::string, std::vector<MessageHandler>> subs_;

    mutable std::mutex                                     pending_mu_;
    std::map<std::string, std::shared_ptr<PendingRequest>> pending_;
};

} // namespace axiom::core