// src/modules/filesystem/filesystem_module.h
//
// FilesystemModule watches configured directories and publishes
// file events onto the event bus.
//
// It uses trailing-edge debouncing: multiple kernel events for the
// same file within a short window are collapsed into one bus event.
// This avoids scanning the same file several times per write, and
// ensures the file is complete when the event is emitted.

#pragma once

#include "core/file_watcher.h"
#include "modules/module_base.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace axiom::core {

class FilesystemModule final : public ModuleBase {
public:
    std::string name() const override { return "filesystem"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    // A pending (debounced) event for a single path.
    struct PendingEvent {
        FileEvent                             event;
        std::chrono::steady_clock::time_point deadline;
    };

    // Watcher callback — called on efsw's thread.
    void on_file_event(const FileEvent& fe);

    // Flusher thread — periodically emits expired events.
    void flush_loop();

    // Publish a single FileEvent to the bus.
    void publish_event(const FileEvent& fe);

    std::unique_ptr<IFileWatcher> watcher_;

    // Debounce state
    std::unordered_map<std::string, PendingEvent> pending_;
    mutable std::mutex                            pending_mutex_;
    std::chrono::milliseconds                     debounce_window_{300};

    // Flusher thread
    std::thread       flush_thread_;
    std::atomic<bool> flush_stop_{false};

    bool running_ = false;
};

} // namespace axiom::core