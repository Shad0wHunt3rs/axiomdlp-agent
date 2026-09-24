// src/modules/inventory/inventory_module.cpp

#include "modules/inventory/inventory_module.h"
#include "modules/inventory/inventory_scanner.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

bool InventoryModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.inventory.enabled) {
        SPDLOG_INFO("inventory module: disabled in config");
        return true;
    }

    interval_ = std::chrono::seconds(ctx_->config.modules.inventory.interval_seconds);
    SPDLOG_INFO("inventory module: initialized (scan every {} s)",
                interval_.count());
    return true;
}

bool InventoryModule::start() {
    if (!ctx_->config.modules.inventory.enabled) {
        return true;
    }

    stop_.store(false, std::memory_order_relaxed);
    scan_thread_ = std::thread([this] { scan_loop(); });
    running_ = true;

    SPDLOG_INFO("inventory module: started");
    return true;
}

void InventoryModule::stop() {
    stop_.store(true, std::memory_order_relaxed);
    if (scan_thread_.joinable()) {
        scan_thread_.join();
    }
    running_ = false;
    SPDLOG_INFO("inventory module: stopped");
}

void InventoryModule::shutdown() {
    SPDLOG_INFO("inventory module: shutting down");
}

HealthStatus InventoryModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    if (!ctx_->config.modules.inventory.enabled) return HealthStatus::ok("disabled");
    if (!running_) return HealthStatus::failed("scanner thread not running");
    return HealthStatus::ok();
}

void InventoryModule::scan_loop() {
    run_one_scan();

    constexpr auto tick = std::chrono::milliseconds(500);
    auto slept          = std::chrono::milliseconds(0);
    const auto target   = std::chrono::duration_cast<std::chrono::milliseconds>(interval_);

    while (!stop_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(tick);
        slept += tick;
        if (slept >= target) {
            if (stop_.load(std::memory_order_relaxed)) break;
            run_one_scan();
            slept = std::chrono::milliseconds(0);
        }
    }
}

void InventoryModule::run_one_scan() {
    InventoryScanner scanner(*ctx_);
    const auto r = scanner.scan_all(stop_);

    last_file_count_ = r.files_seen;

    if (r.was_cancelled) {
        SPDLOG_INFO("inventory scan: cancelled ({} files seen, {} ms)",
                    r.files_seen, r.duration_ms);
    } else {
        SPDLOG_INFO("inventory scan: {} files seen, {} changed, {} deleted, {} ms",
                    r.files_seen, r.files_changed, r.files_deleted, r.duration_ms);
    }
}

} // namespace axiom::core