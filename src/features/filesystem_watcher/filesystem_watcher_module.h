// src/features/filesystem_watcher/filesystem_watcher_module.h
//
// FilesystemWatcherModule watches configured directories in real time
// and publishes file events onto the event bus.
//
// Watcher setup is asynchronous. efsw's recursive watch enumeration
// can take tens of seconds on large trees; blocking the service
// startup on it would make the agent unresponsive to signals during
// that window. Instead, the setup runs on a dedicated thread and the
// module reports "initializing" until it is ready.

#pragma once

#include "platform/file_watcher.h"
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
    void setup_watcher_async();
    void publish_event(const FileEvent& fe);
    void build_excludes();
    bool is_excluded(const std::string& path) const;

    // The watcher is only touched after setup completes, and is
    // protected because setup and stop may run concurrently.
    std::unique_ptr<IFileWatcher> watcher_;
    mutable std::mutex            watcher_mutex_;

    std::vector<std::string> exclude_paths_;

    std::unordered_map<std::string, PendingEvent> pending_;
    mutable std::mutex                            pending_mutex_;
    std::chrono::milliseconds                     debounce_window_{300};

    // Two worker threads: one initializes the watcher, one drains
    // expired events. They run independently of the main service thread.
    std::thread       setup_thread_;
    std::thread       flush_thread_;
    std::atomic<bool> flush_stop_{false};
    std::atomic<bool> setup_done_{false};

    bool running_ = false;
};

} // namespace axiom::core