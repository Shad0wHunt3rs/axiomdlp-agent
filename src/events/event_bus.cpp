// src/core/event_bus.cpp

#include "events/event_bus.h"

#include <spdlog/spdlog.h>

#include <exception>

namespace axiom::core {

SubscriptionId EventBus::subscribe(const std::string& type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_id_++;
    subscriptions_.push_back({id, type, std::move(handler)});
    return id;
}

SubscriptionId EventBus::subscribe_all(EventHandler handler) {
    return subscribe("", std::move(handler));
}

void EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
        if (it->id == id) {
            subscriptions_.erase(it);
            return;
        }
    }
}

void EventBus::publish(EventPtr event) {
    if (!event) {
        return;
    }

    // Copy the matching handlers to a local vector while holding the
    // lock. Then invoke them without the lock. This avoids a deadlock
    // if a handler subscribes or unsubscribes during its call.
    std::vector<EventHandler> to_call;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& sub : subscriptions_) {
            if (sub.type.empty() || sub.type == event->type) {
                to_call.push_back(sub.handler);
            }
        }
    }

    for (const auto& handler : to_call) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("EventBus: subscriber threw: {}", e.what());
        } catch (...) {
            SPDLOG_ERROR("EventBus: subscriber threw an unknown exception");
        }
    }
}

std::size_t EventBus::subscriber_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscriptions_.size();
}

} // namespace axiom::core
