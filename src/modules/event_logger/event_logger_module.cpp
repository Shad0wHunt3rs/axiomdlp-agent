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
            SPDLOG_INFO("[event] type={} id={} path={}",
                        e->type,
                        e->id,
                        e->metadata.count("path") ? e->metadata.at("path") : "");
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
