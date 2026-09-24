// src/platform/common/efsw_file_watcher.cpp
//
// EfswFileWatcher — IFileWatcher implementation backed by the efsw library.
//
// This is the ONLY file in the project that includes <efsw/efsw.hpp>.
// Everything else uses the IFileWatcher interface.
//
// efsw handles the platform differences internally:
//   - Linux:   inotify
//   - Windows: I/O Completion Ports (ReadDirectoryChangesW)
//
// The watcher runs on efsw's own background thread. Our callback is
// invoked on that thread.

#include "core/file_watcher.h"

#include <efsw/efsw.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <system_error>

namespace axiom::core {

namespace {

// ----------------------------------------------------------------
// Map efsw's Action enum to our string event types.
// ----------------------------------------------------------------
const char* action_to_string(efsw::Action action) {
    switch (action) {
        case efsw::Actions::Add:      return file_event_types::kCreated;
        case efsw::Actions::Delete:   return file_event_types::kDeleted;
        case efsw::Actions::Modified: return file_event_types::kModified;
        case efsw::Actions::Moved:    return file_event_types::kMoved;
    }
    return "unknown";
}

// ----------------------------------------------------------------
// The concrete watcher.
//
// Inherits both:
//   - IFileWatcher            (our public interface)
//   - efsw::FileWatchListener (efsw's callback contract)
// ----------------------------------------------------------------
class EfswFileWatcher final : public IFileWatcher,
                              private efsw::FileWatchListener {
public:
    EfswFileWatcher() = default;

    ~EfswFileWatcher() override {
        stop();
    }

    bool start(const std::vector<std::string>& paths,
               Callback callback) override {
        if (running_) {
            SPDLOG_WARN("EfswFileWatcher: already running");
            return false;
        }

        if (paths.empty()) {
            SPDLOG_WARN("EfswFileWatcher: no paths to watch");
            return false;
        }

        callback_ = std::move(callback);
        watcher_  = std::make_unique<efsw::FileWatcher>();

        // Try to watch each path. Skip-with-warning on failure; only
        // fail the whole start() if NO path could be watched.
        //
        // We pre-check existence and directory-ness ourselves, because
        // efsw's addWatch returns a bare negative WatchID with no
        // diagnostic. Without this, "permission denied", "does not
        // exist", and "unsupported filesystem" all produce the same
        // misleading log line.
        std::size_t added              = 0;
        std::size_t skipped_missing    = 0;
        std::size_t skipped_notdir     = 0;
        std::size_t skipped_addwatch   = 0;

        for (const auto& path : paths) {
            std::error_code ec;

            // 1. Exists?
            if (!std::filesystem::exists(path, ec)) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' (does not exist)",
                            path);
                ++skipped_missing;
                continue;
            }

            // 2. Is a directory? efsw cannot watch a regular file, and
            //    silently refuses.
            if (!std::filesystem::is_directory(path, ec)) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' (not a directory)",
                            path);
                ++skipped_notdir;
                continue;
            }

            // 3. Hand it to efsw. If addWatch still refuses, the cause
            //    is genuinely a permission or filesystem limitation,
            //    and we say so plainly.
            const efsw::WatchID id =
                watcher_->addWatch(path, this, /*recursive=*/true);

            if (id < 0) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' "
                            "(permission denied or unsupported filesystem)",
                            path);
                ++skipped_addwatch;
                continue;
            }

            SPDLOG_INFO("EfswFileWatcher: watching '{}' (id={})", path, id);
            ++added;
        }

        if (added == 0) {
            SPDLOG_ERROR("EfswFileWatcher: no paths could be watched");
            watcher_.reset();
            callback_ = nullptr;
            return false;
        }

        watcher_->watch();
        running_ = true;

        SPDLOG_INFO("EfswFileWatcher: started, watching {} path(s) "
                    "({} missing, {} not a directory, {} addWatch failures)",
                    added, skipped_missing, skipped_notdir, skipped_addwatch);
        return true;
    }

    void stop() override {
        if (!running_) {
            return;
        }

        watcher_.reset();
        callback_ = nullptr;
        running_  = false;

        SPDLOG_INFO("EfswFileWatcher: stopped");
    }

private:
    void handleFileAction(efsw::WatchID /*watchid*/,
                          const std::string& dir,
                          const std::string& filename,
                          efsw::Action action,
                          const std::string& old_filename) override {
        FileEvent fe;

        // efsw may or may not include a trailing slash in `dir`.
        // Handle both cases cleanly to avoid producing paths with
        // a double slash (e.g., "/tmp/dlp-test//file.txt").
        if (!dir.empty() && dir.back() == '/') {
            fe.path = dir + filename;
        } else {
            fe.path = dir + "/" + filename;
        }

        fe.event_type = action_to_string(action);
        fe.old_path   = old_filename;
        fe.timestamp  = std::chrono::system_clock::now();

        // Populate is_directory. Consumers (FilesystemModule) use this
        // to skip directory-entry-change noise.
        //
        // efsw does not tell us, so we ask the filesystem ourselves.
        // For Delete actions the path no longer exists, so we leave
        // the flag false - we cannot stat what has been removed, and
        // a false negative here is harmless (the event is still
        // published and can be filtered downstream if needed).
        if (action != efsw::Actions::Delete) {
            std::error_code ec;
            fe.is_directory = std::filesystem::is_directory(fe.path, ec);
        }

        if (callback_) {
            try {
                callback_(fe);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("EfswFileWatcher: callback threw: {}", e.what());
            } catch (...) {
                SPDLOG_ERROR("EfswFileWatcher: callback threw unknown exception");
            }
        }
    }

    std::unique_ptr<efsw::FileWatcher> watcher_;
    Callback                           callback_;
    bool                               running_ = false;
};

} // namespace

std::unique_ptr<IFileWatcher> make_file_watcher() {
    return std::make_unique<EfswFileWatcher>();
}

} // namespace axiom::core