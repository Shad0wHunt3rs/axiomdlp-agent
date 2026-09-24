// src/modules/filesystem/filesystem_module.h
//
// FilesystemModule watches configured directories and publishes
// file events onto the event bus.
//
// It uses trailing-edge debouncing: multiple kernel events for the
// same file within a short window are collapsed into one bus event.
// When two events of different types occur in the same window, the
// higher-priority one (modified/created > moved > deleted) wins.
//
// It also applies an exclusion list. In addition to the user's
// configured exclude_paths, the agent's own log directory is always
// excluded, otherwise each log line produces a new file_write event
// which produces a new log line - an infinite feedback loop.

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
#include <vector>

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
        int                                   priority = 0;
    };

    // Watcher callback - called on efsw's thread.
    void on_file_event(const FileEvent& fe);

    // Flusher thread - periodically emits expired events.
    void flush_loop();

    // Publish a single FileEvent to the bus.
    void publish_event(const FileEvent& fe);

    // Build the effective exclusion list at initialize() time.
    void build_excludes();

    // Does this path match any exclusion pattern?
    bool is_excluded(const std::string& path) const;

    std::unique_ptr<IFileWatcher> watcher_;

    // Exclusions: user-configured + implicit (log dir, data dir).
    std::vector<std::string> exclude_paths_;

    // Debounce state
    std::unordered_map<std::string, PendingEvent> pending_;
    mutable std::mutex                            pending_mutex_;
    std::chrono::milliseconds                     debounce_window_{300};

    // Flusher thread
    std::thread       flush_thread_;
    std::atomic<bool> flush_stop_{false};

    // Limits (consumed by future Scanner milestone).
    int max_file_size_mb_ = 100;

    bool running_ = false;
};

} // namespace axiom::core