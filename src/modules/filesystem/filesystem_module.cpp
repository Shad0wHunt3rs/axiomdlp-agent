// src/modules/filesystem/filesystem_module.cpp

#include "modules/filesystem/filesystem_module.h"

#include "core/event.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <system_error>
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

// Security priority of a file event type. Higher wins when two events
// for the same path land in the same debounce window. Content-bearing
// events (modified, created) outrank deletions, because a deleted file
// can no longer be scanned.
int event_priority(const std::string& file_event_type) {
    if (file_event_type == file_event_types::kModified) return 4;
    if (file_event_type == file_event_types::kCreated)  return 3;
    if (file_event_type == file_event_types::kMoved)    return 2;
    if (file_event_type == file_event_types::kDeleted)  return 1;
    return 0;
}

// Simple glob matcher supporting '*' (matches any sequence of
// characters, including '/') and '?' (matches any single character).
// Not full POSIX glob - no character classes - but covers the patterns
// used in the shipped configs.
bool glob_match(const std::string& pattern, const std::string& text) {
    std::size_t pi = 0, ti = 0;
    std::size_t star_p = std::string::npos, star_t = 0;

    while (ti < text.size()) {
        if (pi < pattern.size() &&
            (pattern[pi] == '?' || pattern[pi] == text[ti])) {
            ++pi; ++ti;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star_p = pi++;
            star_t = ti;
        } else if (star_p != std::string::npos) {
            pi = star_p + 1;
            ti = ++star_t;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

// Does `path` match this exclusion pattern?
// - Direct glob match handles patterns like "*/node_modules/*".
// - Directory-prefix match handles a bare directory like "/var/tmp",
//   so "/var/tmp/anything" is excluded.
bool path_matches_pattern(const std::string& pattern, const std::string& path) {
    if (pattern.empty()) return false;

    if (glob_match(pattern, path)) return true;

    // Directory-prefix form: pattern with no trailing wildcard matches
    // the whole subtree under it.
    if (pattern.back() != '*' && pattern.back() != '?') {
        const std::string prefix = pattern + "/";
        if (path.size() >= prefix.size() &&
            path.compare(0, prefix.size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

// Absolute, lexically-normalized form of `p`. Used so the log-dir
// exclusion works regardless of the CWD the agent started in.
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

bool FilesystemModule::initialize(ModuleContext& context) {
    ModuleBase::initialize(context);

    if (!ctx_->config.modules.filesystem.enabled) {
        SPDLOG_INFO("filesystem module: disabled in config");
        return true;   // not an error - just disabled
    }

    if (ctx_->config.modules.filesystem.watch_paths.empty()) {
        SPDLOG_WARN("filesystem module: enabled but no watch_paths configured");
    }

    // Pull configured values into the module.
    debounce_window_ = std::chrono::milliseconds(
        ctx_->config.modules.filesystem.debounce_ms);
    max_file_size_mb_ = ctx_->config.modules.filesystem.max_file_size_mb;

    build_excludes();

    SPDLOG_INFO("filesystem module: initialized with {} watch path(s), "
                "{} exclude pattern(s), debounce {} ms",
                ctx_->config.modules.filesystem.watch_paths.size(),
                exclude_paths_.size(),
                ctx_->config.modules.filesystem.debounce_ms);
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
    // 1. Stop the watcher first - no more incoming events.
    if (watcher_) {
        watcher_->stop();
        watcher_.reset();
    }

    // 2. Stop the flusher and wait for it to exit.
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
    if (ctx_ == nullptr) {
        // health() may be called before initialize() if this module
        // failed to initialize. Never dereference a null ctx_.
        return HealthStatus::unknown("not initialized");
    }
    if (!ctx_->config.modules.filesystem.enabled) {
        return HealthStatus::ok("disabled");
    }
    if (!running_) {
        return HealthStatus::failed("watcher not running");
    }
    return HealthStatus::ok();
}

// ----------------------------------------------------------------
// Exclusions
// ----------------------------------------------------------------

void FilesystemModule::build_excludes() {
    exclude_paths_.clear();

    // 1. User-configured exclusions.
    for (const auto& p : ctx_->config.modules.filesystem.exclude_paths) {
        exclude_paths_.push_back(p);
    }

    // 2. Implicit: the agent's own log directory.
    //
    //    Without this, the EventLoggerModule's own output re-enters the
    //    pipeline as new file_write events, producing an endless
    //    feedback loop that at best wastes CPU and at worst amplifies
    //    once a Scanner is reading file content.
    if (!ctx_->config.logging.dir.empty()) {
        const std::string log_dir_abs = absolute_lexical(ctx_->config.logging.dir);
        exclude_paths_.push_back(log_dir_abs + "/*");
        SPDLOG_INFO("filesystem module: excluding own log directory '{}'",
                    log_dir_abs);
    }
}

bool FilesystemModule::is_excluded(const std::string& path) const {
    for (const auto& pattern : exclude_paths_) {
        if (path_matches_pattern(pattern, path)) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------
// Debounce: watcher callback
// ----------------------------------------------------------------

void FilesystemModule::on_file_event(const FileEvent& fe) {
    // Directory events are noise for DLP: a directory "modified" event
    // fires when its list of entries changes, not when file content
    // changes. Skip them until we have a reason to care.
    if (fe.is_directory) {
        return;
    }

    // Skip anything matching the exclusion list.
    if (is_excluded(fe.path)) {
        return;
    }

    const int  new_priority = event_priority(fe.event_type);
    const auto now          = std::chrono::steady_clock::now();
    const auto deadline     = now + debounce_window_;

    std::lock_guard<std::mutex> lock(pending_mutex_);

    auto it = pending_.find(fe.path);
    if (it == pending_.end()) {
        // First event for this path in the current window.
        pending_.emplace(fe.path, PendingEvent{fe, deadline, new_priority});
        return;
    }

    // Same path seen before in this window.
    //
    // Only overwrite if the new event is at least as security-relevant
    // as the existing one. This prevents a "deleted" event from
    // clobbering a "modified" event - the modified content is what we
    // need to scan, and the file is gone by the time we'd get to it.
    if (new_priority >= it->second.priority) {
        it->second.event    = fe;
        it->second.deadline = deadline;
        it->second.priority = new_priority;
    } else {
        // Keep the higher-priority event, but extend the deadline so
        // we don't emit it while the file is still being written.
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