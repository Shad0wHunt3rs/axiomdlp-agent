// src/modules/transport/message_router.cpp

#include "net/message_router.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

void MessageRouter::register_handler(const std::string& type, Handler handler) {
    handlers_[type] = std::move(handler);
}

void MessageRouter::dispatch(std::string_view raw_json) {
    nlohmann::json msg;
    try {
        msg = nlohmann::json::parse(raw_json);
    } catch (const std::exception& e) {
        SPDLOG_WARN("MessageRouter: invalid JSON: {}", e.what());
        return;
    }

    if (!msg.is_object() || !msg.contains("type")) {
        SPDLOG_WARN("MessageRouter: missing 'type' field");
        return;
    }

    const std::string type = msg.value("type", "");
    if (type.empty()) {
        SPDLOG_WARN("MessageRouter: empty 'type'");
        return;
    }

    auto it = handlers_.find(type);
    if (it == handlers_.end()) {
        SPDLOG_DEBUG("MessageRouter: no handler for type '{}'", type);
        return;
    }

    const nlohmann::json payload =
        msg.contains("payload") ? msg["payload"] : nlohmann::json::object();

    try {
        it->second(payload);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("MessageRouter: handler for '{}' threw: {}", type, e.what());
    }
}

} // namespace axiom::core