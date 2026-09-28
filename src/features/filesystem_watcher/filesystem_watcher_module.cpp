// src/features/filesystem_watcher/filesystem_watcher_module.cpp

#include "features/filesystem_watcher/filesystem_watcher_module.h"

#include "events/event.h"
#include "util/glob_match.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <system_error>
#include <vector>

namespace axiom::core {

namespace {

std::string to_bus_event_type(const std::string& file_event_type) {
    if (file_event_type == file_event_types::kCreated)  return event_types::kFileWrite;
    if (file_event_type == file_event_types::kModified) return event_types::kFileWrite;
    if (file_event_type == file_event_types::kDeleted)  return event_types::kFileDelete;
    if (file_event_type == file_event_types::kMoved)    return event_types::kFileAccess;
    return "file_event";
}

int event_priority(const std::string& file_event_type) {
    if (file_event_type == file_event_types::kModified) return 4;
    if (file_event_type == file_event_types::kCreated)  return 3;
    if (file_event_type == file_event_types::kMoved)    return 2;
    if (file_event_type == file_event_types::kDeleted)  return 1;
    return 0;
}

std::string absolute_lexical(const std::string& p) {
    std::error_code ec;
    auto abs = std::filesystem::absolute(p, ec);
    if (ec) return p;
    return abs.lexically_normal().string();
}

} // namespace

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------

bool FilesystemWatcherModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.filesystem_watcher.enabled) {
        SPDLOG_INFO("filesystem_watcher module: disabled in config");
        return true;
    }

    if (ctx_->config.modules.filesystem_watcher.watch_paths.empty()) {
        SPDLOG_WARN("filesystem_watcher module: enabled but no watch_paths configured");
    }

    debounce_window_ = std::chrono::milliseconds(
        ctx_->config.modules.filesystem_watcher.debounce_ms);

    build_excludes();

    SPDLOG_INFO("filesystem_watcher module: initialized with {} watch path(s), "
                "{} exclude pattern(s), debounce {} ms",
                ctx_->config.modules.filesystem_watcher.watch_paths.size(),
                exclude_paths_.size(),
                ctx_->config.modules.filesystem_watcher.debounce_ms);
    return true;
}

bool FilesystemWatcherModule::start() {
    if (!ctx_->config.modules.filesystem_watcher.enabled) {
        return true;
    }

    flush_stop_.store(false, std::memory_order_relaxed);
    flush_thread_ = std::thread([this] { flush_loop(); });

    setup_done_.store(false, std::memory_order_relaxed);
    setup_thread_ = std::thread([this] { setup_watcher_async(); });

    running_ = true;
    SPDLOG_INFO("filesystem_watcher module: started (watcher initializing)");
    return true;
}

void FilesystemWatcherModule::setup_watcher_async() {
    auto w = make_file_watcher();
    if (!w) {
        SPDLOG_ERROR("filesystem_watcher module: failed to create file watcher");
        setup_done_.store(true, std::memory_order_relaxed);
        return;
    }

    const auto& paths = ctx_->config.modules.filesystem_watcher.watch_paths;

    SPDLOG_INFO("filesystem_watcher module: initializing watcher over {} path(s)",
                paths.size());

    const bool ok = w->start(
        paths,
        [this](const FileEvent& fe) { on_file_event(fe); });

    if (!ok) {
        SPDLOG_ERROR("filesystem_watcher module: watcher failed to start");
        setup_done_.store(true, std::memory_order_relaxed);
        return;
    }

    if (flush_stop_.load(std::memory_order_relaxed)) {
        w->stop();
        setup_done_.store(true, std::memory_order_relaxed);
        SPDLOG_INFO("filesystem_watcher module: watcher setup cancelled");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(watcher_mutex_);
        watcher_ = std::move(w);
    }
    setup_done_.store(true, std::memory_order_relaxed);
    SPDLOG_INFO("filesystem_watcher module: watcher ready");
}

void FilesystemWatcherModule::stop() {
    flush_stop_.store(true, std::memory_order_relaxed);

    if (setup_thread_.joinable()) {
        if (!setup_done_.load(std::memory_order_relaxed)) {
            SPDLOG_INFO("filesystem_watcher module: waiting for watcher setup to complete...");
        }
        setup_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(watcher_mutex_);
        if (watcher_) {
            watcher_->stop();
            watcher_.reset();
        }
    }

    if (flush_thread_.joinable()) {
        flush_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (auto& [path, pending] : pending_) {
            publish_event(pending.event);
        }
        pending_.clear();
    }

    running_ = false;
    SPDLOG_INFO("filesystem_watcher module: stopped");
}

void FilesystemWatcherModule::shutdown() {
    SPDLOG_INFO("filesystem_watcher module: shutting down");
}

HealthStatus FilesystemWatcherModule::health() const {
    if (ctx_ == nullptr) {
        return HealthStatus::unknown("not initialized");
    }
    if (!ctx_->config.modules.filesystem_watcher.enabled) {
        return HealthStatus::ok("disabled");
    }
    if (!running_) {
        return HealthStatus::failed("watcher not running");
    }
    if (!setup_done_.load(std::memory_order_relaxed)) {
        return HealthStatus::degraded("watcher initializing");
    }
    return HealthStatus::ok();
}

// ----------------------------------------------------------------
// Exclusions
// ----------------------------------------------------------------

void FilesystemWatcherModule::build_excludes() {
    exclude_paths_.clear();

    for (const auto& p : ctx_->config.modules.filesystem_watcher.exclude_paths) {
        exclude_paths_.push_back(p);
    }

    if (!ctx_->config.logging.dir.empty()) {
        const std::string log_dir_abs = absolute_lexical(ctx_->config.logging.dir);
        exclude_paths_.push_back(log_dir_abs + "/*");
        SPDLOG_INFO("filesystem_watcher module: excluding own log directory '{}'",
                    log_dir_abs);
    }

    if (!ctx_->config.storage.db_path.empty()) {
        const auto db_dir = std::filesystem::path(ctx_->config.storage.db_path)
                                .parent_path().string();
        if (!db_dir.empty()) {
            const std::string db_dir_abs = absolute_lexical(db_dir);
            exclude_paths_.push_back(db_dir_abs + "/*");
            SPDLOG_INFO("filesystem_watcher module: excluding own data directory '{}'",
                        db_dir_abs);
        }
    }
}

bool FilesystemWatcherModule::is_excluded(const std::string& path) const {
    for (const auto& pattern : exclude_paths_) {
        if (util::path_matches_pattern(pattern, path)) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------
// Debounce: watcher callback
// ----------------------------------------------------------------

void FilesystemWatcherModule::on_file_event(const FileEvent& fe) {
    if (fe.is_directory) {
        return;
    }

    if (is_excluded(fe.path)) {
        return;
    }

    const int  new_priority = event_priority(fe.event_type);
    const auto now          = std::chrono::steady_clock::now();
    const auto deadline     = now + debounce_window_;

    std::lock_guard<std::mutex> lock(pending_mutex_);

    auto it = pending_.find(fe.path);
    if (it == pending_.end()) {
        pending_.emplace(fe.path, PendingEvent{fe, deadline, new_priority});
        return;
    }

    if (new_priority >= it->second.priority) {
        it->second.event    = fe;
        it->second.deadline = deadline;
        it->second.priority = new_priority;
    } else {
        it->second.deadline = deadline;
    }
}

// ----------------------------------------------------------------
// Flusher thread
// ----------------------------------------------------------------

void FilesystemWatcherModule::flush_loop() {
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

void FilesystemWatcherModule::publish_event(const FileEvent& fe) {
    auto e = std::make_shared<Event>(Event::create(to_bus_event_type(fe.event_type)));

    e->metadata["path"]      = fe.path;
    e->metadata["source"]    = "filesystem_watcher";
    e->metadata["fs_action"] = fe.event_type;
    if (!fe.old_path.empty()) {
        e->metadata["old_path"] = fe.old_path;
    }

    ctx_->event_bus.publish(e);
}

} // namespace axiom::core