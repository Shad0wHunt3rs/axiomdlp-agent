// src/modules/filesystem/filesystem_module.cpp

#include "modules/filesystem/filesystem_module.h"

#include "core/event.h"

#include <spdlog/spdlog.h>

#include <vector>

namespace axiom::core {

namespace {

// Translate a FileEvent's event_type ("created", "modified", ...) into
// a bus event type ("file_write", "file_delete", ...).
std::string to_bus_event_type(const std::string& file_event_type) {
    if (file_event_type == file_event_types::kCreated)  return event_types::kFileWrite;
    if (file_event_type == file_event_types::kModified) return event_types::kFileWrite;
    if (file_event_type == file_event_types::kDeleted)  return event_types::kFileDelete;
    if (file_event_type == file_event_types::kMoved)    return event_types::kFileAccess;
    return "file_event";
}

} // namespace

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------

bool FilesystemModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.filesystem.enabled) {
        SPDLOG_INFO("filesystem module: disabled in config");
        return true;   // not an error — just disabled
    }

    if (ctx_->config.modules.filesystem.watch_paths.empty()) {
        SPDLOG_WARN("filesystem module: enabled but no watch_paths configured");
    }

    SPDLOG_INFO("filesystem module: initialized with {} watch path(s)",
                ctx_->config.modules.filesystem.watch_paths.size());
    return true;
}

bool FilesystemModule::start() {
    if (!ctx_->config.modules.filesystem.enabled) {
        return true;   // disabled, nothing to start
    }

    // Start the flusher thread.
    flush_stop_.store(false, std::memory_order_relaxed);
    flush_thread_ = std::thread([this] { flush_loop(); });

    // Create and start the watcher.
    watcher_ = make_file_watcher();
    if (!watcher_) {
        SPDLOG_ERROR("filesystem module: failed to create file watcher");
        flush_stop_.store(true, std::memory_order_relaxed);
        if (flush_thread_.joinable()) flush_thread_.join();
        return false;
    }

    const auto& paths = ctx_->config.modules.filesystem.watch_paths;

    const bool ok = watcher_->start(
        paths,
        [this](const FileEvent& fe) { on_file_event(fe); });

    if (!ok) {
        SPDLOG_ERROR("filesystem module: watcher failed to start");
        watcher_.reset();
        flush_stop_.store(true, std::memory_order_relaxed);
        if (flush_thread_.joinable()) flush_thread_.join();
        return false;
    }

    running_ = true;
    SPDLOG_INFO("filesystem module: started (debounce {} ms)",
                debounce_window_.count());
    return true;
}

void FilesystemModule::stop() {
    // 1. Stop the watcher first — no more incoming events.
    if (watcher_) {
        watcher_->stop();
        watcher_.reset();
    }

    // 2. Stop the flusher.
    flush_stop_.store(true, std::memory_order_relaxed);
    if (flush_thread_.joinable()) {
        flush_thread_.join();
    }

    // 3. Flush any remaining pending events.
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (auto& [path, pending] : pending_) {
            publish_event(pending.event);
        }
        pending_.clear();
    }

    running_ = false;
    SPDLOG_INFO("filesystem module: stopped");
}

void FilesystemModule::shutdown() {
    SPDLOG_INFO("filesystem module: shutting down");
}

HealthStatus FilesystemModule::health() const {
    if (!ctx_->config.modules.filesystem.enabled) {
        return HealthStatus::ok("disabled");
    }
    if (!running_) {
        return HealthStatus::failed("watcher not running");
    }
    return HealthStatus::ok();
}

// ----------------------------------------------------------------
// Debounce: watcher callback
// ----------------------------------------------------------------

void FilesystemModule::on_file_event(const FileEvent& fe) {
    const auto now      = std::chrono::steady_clock::now();
    const auto deadline = now + debounce_window_;

    std::lock_guard<std::mutex> lock(pending_mutex_);

    auto it = pending_.find(fe.path);
    if (it == pending_.end()) {
        // First event for this path in the current window.
        pending_.emplace(fe.path, PendingEvent{fe, deadline});
    } else {
        // Refresh: keep the newest event, reset the deadline.
        it->second.event    = fe;
        it->second.deadline = deadline;
    }
}

// ----------------------------------------------------------------
// Flusher thread
// ----------------------------------------------------------------

void FilesystemModule::flush_loop() {
    constexpr auto tick = std::chrono::milliseconds(50);

    while (!flush_stop_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(tick);

        std::vector<FileEvent> to_publish;

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            const auto now = std::chrono::steady_clock::now();

            for (auto it = pending_.begin(); it != pending_.end(); ) {
                if (it->second.deadline <= now) {
                    to_publish.push_back(it->second.event);
                    it = pending_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (const auto& fe : to_publish) {
            publish_event(fe);
        }
    }
}

// ----------------------------------------------------------------
// Publish one FileEvent to the bus
// ----------------------------------------------------------------

void FilesystemModule::publish_event(const FileEvent& fe) {
    auto e = std::make_shared<Event>(Event::create(to_bus_event_type(fe.event_type)));

    e->metadata["path"]      = fe.path;
    e->metadata["source"]    = "filesystem";
    e->metadata["fs_action"] = fe.event_type;
    if (!fe.old_path.empty()) {
        e->metadata["old_path"] = fe.old_path;
    }

    ctx_->event_bus.publish(e);
}

} // namespace axiom::core