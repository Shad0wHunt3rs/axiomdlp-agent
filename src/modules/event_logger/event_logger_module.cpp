// src/modules/event_logger/event_logger_module.cpp

#include "modules/event_logger/event_logger_module.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

bool EventLoggerModule::start() {
    if (ctx_ == nullptr) {
        SPDLOG_ERROR("event_logger: no module context");
        return false;
    }

    subscription_id_ = ctx_->event_bus.subscribe_all(
        [](const EventPtr& e) {
            // INFO, not DEBUG: the event stream is the agent's primary
            // output during development and is expected to be visible
            // by default. The filesystem module excludes the agent's
            // own log directory, so writing these lines does NOT feed
            // back into the pipeline as new file events.
            //
            // Look up "path" once and hold a const char* rather than
            // building a temporary std::string via the ternary. This
            // keeps the per-event cost minimal even at high rates.
            const char* path = "-";
            const auto it = e->metadata.find("path");
            if (it != e->metadata.end()) {
                path = it->second.c_str();
            }

            SPDLOG_INFO("[event] type={} path={}", e->type, path);
        });

    SPDLOG_INFO("event_logger: subscribed to all events (id={})", subscription_id_);
    return true;
}

void EventLoggerModule::stop() {
    if (ctx_ != nullptr && subscription_id_ != 0) {
        ctx_->event_bus.unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
    SPDLOG_INFO("event_logger: unsubscribed");
}

} // namespace axiom::core