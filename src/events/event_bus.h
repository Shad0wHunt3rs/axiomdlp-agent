// src/core/event_bus.h
//
// EventBus is a synchronous publish/subscribe system.
//
// Modules publish events onto the bus. Other modules subscribe to
// event types (or to all events) and receive callbacks when a matching
// event is published.
//
// Publishers and subscribers never know about each other. This is
// what decouples modules from each other.
//
// Thread safety:
//   subscribe / unsubscribe / publish may be called from any thread.
//   Callbacks run on the publishing thread. A slow subscriber can
//   delay other subscribers of the same event.

#pragma once

#include "events/event.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace axiom::core {

using EventPtr     = std::shared_ptr<Event>;
using EventHandler = std::function<void(const EventPtr&)>;
using SubscriptionId = std::uint64_t;

class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    // Non-copyable, non-movable.
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    // Subscribe to a specific event type.
    // Returns a subscription ID that can be used to unsubscribe.
    SubscriptionId subscribe(const std::string& type, EventHandler handler);

    // Subscribe to every event, regardless of type.
    SubscriptionId subscribe_all(EventHandler handler);

    // Remove a subscription. Safe to call with an unknown ID (no-op).
    void unsubscribe(SubscriptionId id);

    // Publish an event to all matching subscribers.
    // Subscribers are called synchronously, on the caller's thread.
    // Exceptions thrown by subscribers are caught and logged.
    void publish(EventPtr event);

    // Number of active subscriptions (useful for diagnostics).
    std::size_t subscriber_count() const;

private:
    struct Subscription {
        SubscriptionId id;
        std::string    type;      // empty string = all events
        EventHandler   handler;
    };

    mutable std::mutex            mutex_;
    std::vector<Subscription>     subscriptions_;
    SubscriptionId                next_id_ = 1;
};

} // namespace axiom::core
