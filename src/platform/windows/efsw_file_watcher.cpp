// src/platform/windows/efsw_file_watcher.cpp

#ifdef _WIN32

#include "platform/file_watcher.h"
#include <efsw/efsw.hpp>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <memory>
#include <system_error>

namespace axiom::core {

namespace {
const char* action_to_string(efsw::Action action) {
    switch (action) {
        case efsw::Actions::Add:      return file_event_types::kCreated;
        case efsw::Actions::Delete:   return file_event_types::kDeleted;
        case efsw::Actions::Modified: return file_event_types::kModified;
        case efsw::Actions::Moved:    return file_event_types::kMoved;
    }
    return "unknown";
}

class EfswFileWatcher final : public IFileWatcher,
                              private efsw::FileWatchListener {
public:
    EfswFileWatcher() = default;
    ~EfswFileWatcher() override { stop(); }

    bool start(const std::vector<std::string>& paths, Callback callback) override {
        if (running_) { SPDLOG_WARN("EfswFileWatcher: already running"); return false; }
        if (paths.empty()) { SPDLOG_WARN("EfswFileWatcher: no paths to watch"); return false; }

        callback_ = std::move(callback);
        watcher_  = std::make_unique<efsw::FileWatcher>();

        std::size_t added = 0;
        for (const auto& path : paths) {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' (does not exist)", path);
                continue;
            }
            if (!std::filesystem::is_directory(path, ec)) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' (not a directory)", path);
                continue;
            }
            const efsw::WatchID id = watcher_->addWatch(path, this, /*recursive=*/true);
            if (id < 0) {
                SPDLOG_WARN("EfswFileWatcher: skipping '{}' (permission denied)", path);
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
        return true;
    }

    void stop() override {
        if (!running_) return;
        watcher_.reset();
        callback_ = nullptr;
        running_  = false;
    }

private:
    // Windows efsw 1.5.0 uses std::string by value
    void handleFileAction(efsw::WatchID,
                          const std::string& dir,
                          const std::string& filename,
                          efsw::Action action,
                          std::string old_filename) override {
        FileEvent fe;
        fe.path = (!dir.empty() && dir.back() == '/') ? dir + filename : dir + "/" + filename;
        fe.event_type = action_to_string(action);
        fe.old_path   = old_filename;
        fe.timestamp  = std::chrono::system_clock::now();

        if (action != efsw::Actions::Delete) {
            std::error_code ec;
            fe.is_directory = std::filesystem::is_directory(fe.path, ec);
        }

        if (callback_) {
            try { callback_(fe); }
            catch (const std::exception& e) { SPDLOG_ERROR("EfswFileWatcher: callback threw: {}", e.what()); }
            catch (...) { SPDLOG_ERROR("EfswFileWatcher: callback threw unknown exception"); }
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

#endif // _WIN32