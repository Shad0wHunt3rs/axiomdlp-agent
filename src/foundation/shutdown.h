// src/foundation/shutdown.h
//
// ShutdownController is the single, thread-safe authority on whether
// the agent is shutting down and why.
//
// Any module can call request(reason). The first call wins; later
// calls are no-ops. The service main loop blocks on wait() and tears
// down the whole agent as soon as a request arrives. The reason is
// readable everywhere via reason() so modules can act accordingly —
// for example, the transport module sends a "goodbye" message to the
// server only on graceful shutdowns.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace axiom::core {

enum class ShutdownReason {
    None,              // not shutting down (initial state)
    UserRequested,     // Ctrl+C, SIGTERM, or service stop
    AuthRevoked,       // 401 from server: token invalid or revoked
    AgentRevoked,      // 403 from server: this specific agent revoked
    ConfigError,       // bad config at startup
    FatalError,        // unrecoverable error in a module
    ServerShutdown     // server sent 1001 Going Away
};

inline const char* to_string(ShutdownReason r) {
    switch (r) {
        case ShutdownReason::None:           return "none";
        case ShutdownReason::UserRequested:  return "user_requested";
        case ShutdownReason::AuthRevoked:    return "auth_revoked";
        case ShutdownReason::AgentRevoked:   return "agent_revoked";
        case ShutdownReason::ConfigError:    return "config_error";
        case ShutdownReason::FatalError:     return "fatal_error";
        case ShutdownReason::ServerShutdown: return "server_shutdown";
    }
    return "unknown";
}

class ShutdownController {
public:
    ShutdownController() = default;

    ShutdownController(const ShutdownController&) = delete;
    ShutdownController& operator=(const ShutdownController&) = delete;
    ShutdownController(ShutdownController&&) = delete;
    ShutdownController& operator=(ShutdownController&&) = delete;

    // Request a shutdown. The first caller wins; later calls are no-ops.
    // Returns true if this call triggered the shutdown.
    bool request(ShutdownReason reason) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (requested_) return false;
            reason_    = reason;
            requested_ = true;
        }
        cv_.notify_all();
        return true;
    }

    // Block until a shutdown has been requested.
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return requested_; });
    }

    // Block until a shutdown has been requested, or until the timeout
    // expires. Returns true if shutdown was requested.
    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return requested_; });
    }

    bool is_requested() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return requested_;
    }

    ShutdownReason reason() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return reason_;
    }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    bool                    requested_ = false;
    ShutdownReason          reason_    = ShutdownReason::None;
};

} // namespace axiom::core