// src/modules/example/example_module.cpp

#include "modules/example/example_module.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

namespace {
constexpr const char* kPingEvent = "example_ping";
}

bool ExampleModule::initialize(ModuleContext& context) {
    if (!ModuleBase::initialize(context)) {
        return false;
    }
    SPDLOG_INFO("example module: initializing");
    return true;
}

bool ExampleModule::start() {
    SPDLOG_INFO("example module: starting");

    if (ctx_ == nullptr) {
        SPDLOG_ERROR("example module: no module context");
        return false;
    }

    subscription_id_ = ctx_->event_bus.subscribe(
        kPingEvent,
        [](const EventPtr& e) {
            SPDLOG_INFO("example module: received ping (id={})", e->id);
        });

    SPDLOG_INFO("example module: subscribed (id={})", subscription_id_);

    auto e = std::make_shared<Event>(Event::create(kPingEvent));
    e->metadata["source"] = "example_module";
    ctx_->event_bus.publish(e);

    return true;
}

void ExampleModule::stop() {
    SPDLOG_INFO("example module: stopping");

    if (ctx_ != nullptr && subscription_id_ != 0) {
        ctx_->event_bus.unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
}

void ExampleModule::shutdown() {
    SPDLOG_INFO("example module: shutting down");
}

} // namespace axiom::core