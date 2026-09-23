// src/core/health.h
//
// Health status types used by modules and the module manager.
//
// Every module reports a HealthStatus. The manager aggregates them
// into a single view for the whole agent.

#pragma once

#include <string>

namespace axiom::core {

// The four levels of health a component can report.
enum class HealthLevel {
    Unknown,   // Not yet checked, or unable to determine
    OK,        // Fully working
    Degraded,  // Working but with issues
    Failed,    // Not working
};

// A snapshot of a component's health.
struct HealthStatus {
    HealthLevel level = HealthLevel::Unknown;
    std::string message;

    // Convenience constructors for the common cases.
    static HealthStatus ok(const std::string& msg = "") {
        return {HealthLevel::OK, msg};
    }
    static HealthStatus degraded(const std::string& msg) {
        return {HealthLevel::Degraded, msg};
    }
    static HealthStatus failed(const std::string& msg) {
        return {HealthLevel::Failed, msg};
    }

    // Helper: is this status considered healthy?
    bool is_healthy() const {
        return level == HealthLevel::OK || level == HealthLevel::Degraded;
    }
};

// Human-readable name for a level (for logs and health reports).
inline const char* to_string(HealthLevel level) {
    switch (level) {
        case HealthLevel::OK:       return "OK";
        case HealthLevel::Degraded: return "Degraded";
        case HealthLevel::Failed:   return "Failed";
        case HealthLevel::Unknown:  return "Unknown";
    }
    return "Unknown";
}

} // namespace axiom::core
