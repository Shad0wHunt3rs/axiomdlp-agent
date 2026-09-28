// src/modules/transport/message_router.h
//
// MessageRouter dispatches incoming JSON messages to handlers based on
// their "type" field. Handlers are registered by string type; the
// router knows nothing about specific types.
//
// This is what makes the transport future-proof: adding a new server
// message type is a registration call from wherever the handler lives,
// not a change to the transport itself.

#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <string_view>   // FIX: explicit include; was relying on
                         // <nlohmann/json.hpp> to pull it in transitively
#include <unordered_map>

namespace axiom::core {

class MessageRouter {
public:
    using Handler = std::function<void(const nlohmann::json& payload)>;

    // Register a handler for a specific message type. Overwrites any
    // existing handler for the same type.
    void register_handler(const std::string& type, Handler handler);

    // Parse a JSON message and dispatch to the registered handler.
    // Unknown types are logged and ignored.
    void dispatch(std::string_view raw_json);

private:
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace axiom::core