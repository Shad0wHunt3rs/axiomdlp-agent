// src/modules/metadata/metadata_module.h
//
// MetadataModule schedules periodic filesystem walks. It owns a
// background thread that calls the Walker on an interval, and reports
// walk statistics through the log.

#pragma once

#include "modules/module_base.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

namespace axiom::core {

class MetadataModule final : public ModuleBase {
public:
    std::string name() const override { return "metadata"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    void walk_loop();
    void run_one_walk();

    std::thread       walk_thread_;
    std::atomic<bool> stop_{false};

    std::chrono::seconds interval_{900};
    std::size_t          last_file_count_ = 0;
    bool                 running_ = false;
};

} // namespace axiom::core