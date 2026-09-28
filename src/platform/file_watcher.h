// src/core/file_watcher.h
//
// IFileWatcher — platform-agnostic file system watching interface.
//
// Two responsibilities:
//   1. Watch a list of directories for filesystem changes.
//   2. Call a user-provided callback for every change event.
//
// The concrete implementation lives elsewhere (currently
// platform/common/efsw_file_watcher.cpp, using the efsw library).
//
// Design notes:
//   - The watcher runs on its own background thread.
//   - The callback is invoked on that thread.
//   - stop() joins the thread.
//   - One watcher may watch multiple directories at once.

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace axiom::core {

// ----------------------------------------------------------------
// FileEvent — a single filesystem change
// ----------------------------------------------------------------

// What kind of change occurred.
// We use strings (not an enum) so that the vocabulary is open and
// matches the stringly-typed event bus.
namespace file_event_types {
inline constexpr const char* kCreated  = "created";
inline constexpr const char* kModified = "modified";
inline constexpr const char* kDeleted  = "deleted";
inline constexpr const char* kMoved    = "moved";
}

struct FileEvent {
    // Full path of the affected file (or directory).
    std::string path;

    // One of the file_event_types:: constants above.
    std::string event_type;

    // For "moved" events, the previous path. Empty otherwise.
    std::string old_path;

    // True if the affected entity is a directory.
    bool is_directory = false;

    // When the event was observed by the watcher.
    std::chrono::system_clock::time_point timestamp;
};

// ----------------------------------------------------------------
// IFileWatcher — the interface
// ----------------------------------------------------------------

class IFileWatcher {
public:
    using Callback = std::function<void(const FileEvent&)>;

    virtual ~IFileWatcher() = default;

    // Non-copyable, non-movable — owns a background thread.
    IFileWatcher(const IFileWatcher&) = delete;
    IFileWatcher& operator=(const IFileWatcher&) = delete;
    IFileWatcher(IFileWatcher&&) = delete;
    IFileWatcher& operator=(IFileWatcher&&) = delete;

    // Begin watching the given directories recursively.
    // Spawns a background thread. Returns true on success.
    // The callback is invoked (on the watcher thread) for every event.
    virtual bool start(const std::vector<std::string>& paths,
                       Callback callback) = 0;

    // Stop watching and join the background thread.
    // Safe to call multiple times. After stop(), the watcher can be
    // start()ed again, but most implementations will prefer to be
    // destroyed and recreated.
    virtual void stop() = 0;

protected:
    // Protected constructor so only derived classes can be created.
    IFileWatcher() = default;
};

// ----------------------------------------------------------------
// Factory
// ----------------------------------------------------------------

// Create the platform-appropriate file watcher implementation.
// Currently returns an efsw-backed watcher that works on both Linux
// and Windows. Later milestones may add platform-specific backends.
std::unique_ptr<IFileWatcher> make_file_watcher();

} // namespace axiom::core