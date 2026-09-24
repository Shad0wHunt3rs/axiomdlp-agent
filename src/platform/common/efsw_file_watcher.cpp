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

#include <memory>

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

        // Try to watch each path. If one fails (permission denied,
        // path does not exist, unsupported filesystem, etc.), log a
        // warning and continue with the others. Only fail if NO path
        // could be watched.
        std::size_t added  = 0;
        std::size_t failed = 0;

        for (const auto& path : paths) {
            const efsw::WatchID id =
                watcher_->addWatch(path, this, /*recursive=*/true);

            if (id < 0) {
                SPDLOG_WARN("EfswFileWatcher: cannot watch '{}' "
                            "(permission denied, missing, or unsupported); skipping",
                            path);
                ++failed;
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
                    "({} skipped)", added, failed);
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