// src/modules/metadata/metadata_module.cpp

#include "modules/metadata/metadata_module.h"
#include "modules/metadata/walker.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

bool MetadataModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.metadata.enabled) {
        SPDLOG_INFO("metadata module: disabled in config");
        return true;
    }

    interval_ = std::chrono::seconds(ctx_->config.modules.metadata.interval_seconds);
    SPDLOG_INFO("metadata module: initialized (walk every {} s)",
                interval_.count());
    return true;
}

bool MetadataModule::start() {
    if (!ctx_->config.modules.metadata.enabled) {
        return true;
    }

    stop_.store(false, std::memory_order_relaxed);
    walk_thread_ = std::thread([this] { walk_loop(); });
    running_ = true;

    SPDLOG_INFO("metadata module: started");
    return true;
}

void MetadataModule::stop() {
    stop_.store(true, std::memory_order_relaxed);
    if (walk_thread_.joinable()) walk_thread_.join();
    running_ = false;
    SPDLOG_INFO("metadata module: stopped");
}

void MetadataModule::shutdown() {
    SPDLOG_INFO("metadata module: shutting down");
}

HealthStatus MetadataModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!ctx_->config.modules.metadata.enabled) return HealthStatus::ok("disabled");
    if (!running_) return HealthStatus::failed("walker thread not running");
    return HealthStatus::ok();
}

void MetadataModule::walk_loop() {
    // First walk immediately.
    run_one_walk();

    constexpr auto tick = std::chrono::milliseconds(500);
    auto slept          = std::chrono::milliseconds(0);
    const auto target   = std::chrono::duration_cast<std::chrono::milliseconds>(interval_);

    while (!stop_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(tick);
        slept += tick;
        if (slept >= target) {
            if (stop_.load(std::memory_order_relaxed)) break;
            run_one_walk();
            slept = std::chrono::milliseconds(0);
        }
    }
}

void MetadataModule::run_one_walk() {
    Walker walker(*ctx_);
    const auto r = walker.walk_all();
    last_file_count_ = r.files_seen;

    SPDLOG_INFO("metadata walk: {} files seen, {} changed, {} deleted, {} ms",
                r.files_seen, r.files_changed, r.files_deleted, r.duration_ms);
}

} // namespace axiom::core