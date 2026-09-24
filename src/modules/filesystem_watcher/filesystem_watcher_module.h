// src/modules/filesystem_watcher/filesystem_watcher_module.h
//
// FilesystemWatcherModule watches configured directories in real time
// and publishes file events onto the event bus.
//
// It uses trailing-edge debouncing: multiple kernel events for the
// same file within a short window are collapsed into one bus event.
// When two events of different types occur in the same window, the
// higher-priority one (modified/created > moved > deleted) wins.
//
// It also applies an exclusion list. In addition to the user's
// configured exclude_paths, the agent's own log and data directories
// are always excluded - otherwise each log line or database write
// produces a new file event, which produces a new log line - an
// infinite feedback loop.

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

class FilesystemWatcherModule final : public ModuleBase {
public:
    std::string name() const override { return "filesystem_watcher"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    struct PendingEvent {
        FileEvent                             event;
        std::chrono::steady_clock::time_point deadline;
        int                                   priority = 0;
    };

    void on_file_event(const FileEvent& fe);
    void flush_loop();
    void publish_event(const FileEvent& fe);
    void build_excludes();
    bool is_excluded(const std::string& path) const;

    std::unique_ptr<IFileWatcher> watcher_;

    std::vector<std::string> exclude_paths_;

    std::unordered_map<std::string, PendingEvent> pending_;
    mutable std::mutex                            pending_mutex_;
    std::chrono::milliseconds                     debounce_window_{300};

    std::thread       flush_thread_;
    std::atomic<bool> flush_stop_{false};

    int max_file_size_mb_ = 100;

    bool running_ = false;
};

} // namespace axiom::core