// src/modules/inventory/inventory_module.h
//
// InventoryModule schedules periodic filesystem scans. It owns a
// background thread that calls the InventoryScanner on an interval,
// and reports scan statistics through the log.

#pragma once

#include "modules/module_base.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

namespace axiom::core {

class InventoryModule final : public ModuleBase {
public:
    std::string name() const override { return "inventory"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    void scan_loop();
    void run_one_scan();

    std::thread       scan_thread_;
    std::atomic<bool> stop_{false};

    std::chrono::seconds interval_{900};
    std::size_t          last_file_count_ = 0;
    bool                 running_ = false;
};

} // namespace axiom::core