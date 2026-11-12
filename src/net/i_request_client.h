// src/net/i_request_client.h
//
// IRequestClient is the transport-agnostic request/response interface used
// by feature modules (agent_sync, content_scanner) to reach the Core.
//
// The concrete implementation today is MqttTransportModule. Adding a new
// transport (or swapping to a mock in tests) does not require touching the
// feature modules.

#pragma once

#include <functional>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace axiom::core {

class IRequestClient {
public:
    virtual ~IRequestClient() = default;

    // Send `payload` to `topic` and block until the response arrives.
    // Returns std::nullopt on timeout, publish failure, or when the
    // transport is not connected.
    //
    // The implementation is responsible for adding "v" and
    // "correlation_id" to the payload and for correlating the response.
    virtual std::optional<nlohmann::json> request(
        const std::string&    topic,
        const nlohmann::json& payload,
        int                   qos,
        int                   timeout_seconds) = 0;

    // Register a handler for server-initiated frames on a specific topic.
    // Multiple handlers per topic are allowed.
    virtual void subscribe(
        const std::string&                        topic,
        std::function<void(const nlohmann::json&)> handler) = 0;

    // Whether the transport is connected and can send requests. The
    // feature modules use this to avoid hammering request() during the
    // first few seconds of startup while the transport is still dialing
    // the broker.
    virtual bool is_connected() const = 0;

    // The enrollment flow learns the agent_id from the server's register
    // response. The transport uses it to subscribe to the agent-scoped
    // topics (command, policy, revoke). Passing 0 clears the identity
    // (used when the server reports not_found on a heartbeat and the
    // agent needs to re-register).
    virtual void set_agent_id(int agent_id) = 0;
};

} // namespace axiom::core