// src/core/module_manager.cpp

#include "core/module_manager.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

// ----------------------------------------------------------------
// Registration
// ----------------------------------------------------------------

void ModuleManager::register_module(std::unique_ptr<IModule> module) {
    if (!module) {
        SPDLOG_WARN("ModuleManager: attempting to register a null module");
        return;
    }

    SPDLOG_INFO("ModuleManager: registering module '{}' v{}",
                module->name(), module->version());

    modules_.push_back(std::move(module));
}

// ----------------------------------------------------------------
// Lifecycle — initialization
// ----------------------------------------------------------------

bool ModuleManager::initialize_all(ModuleContext& context) {
    if (modules_.empty()) {
        SPDLOG_WARN("ModuleManager: no modules registered");
        return true;
    }

    SPDLOG_INFO("ModuleManager: initializing {} module(s)", modules_.size());

    bool all_ok = true;
    for (auto& m : modules_) {
        SPDLOG_INFO("ModuleManager: initializing '{}'", m->name());

        if (!m->initialize(context)) {
            SPDLOG_ERROR("ModuleManager: module '{}' failed to initialize",
                         m->name());
            all_ok = false;
        }
    }

    return all_ok;
}

// ----------------------------------------------------------------
// Lifecycle — start
// ----------------------------------------------------------------

bool ModuleManager::start_all() {
    if (modules_.empty()) {
        SPDLOG_WARN("ModuleManager: no modules to start");
        return true;
    }

    SPDLOG_INFO("ModuleManager: starting {} module(s)", modules_.size());

    bool all_ok = true;
    for (auto& m : modules_) {
        SPDLOG_INFO("ModuleManager: starting '{}'", m->name());

        if (!m->start()) {
            SPDLOG_ERROR("ModuleManager: module '{}' failed to start",
                         m->name());
            all_ok = false;
        }
    }

    return all_ok;
}

// ----------------------------------------------------------------
// Lifecycle — stop (reverse order)
// ----------------------------------------------------------------

void ModuleManager::stop_all() {
    if (modules_.empty()) {
        return;
    }

    SPDLOG_INFO("ModuleManager: stopping {} module(s)", modules_.size());

    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        auto& m = *it;
        SPDLOG_INFO("ModuleManager: stopping '{}'", m->name());
        m->stop();
    }
}

// ----------------------------------------------------------------
// Lifecycle — shutdown (reverse order)
// ----------------------------------------------------------------

void ModuleManager::shutdown_all() {
    if (modules_.empty()) {
        return;
    }

    SPDLOG_INFO("ModuleManager: shutting down {} module(s)", modules_.size());

    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        auto& m = *it;
        SPDLOG_INFO("ModuleManager: shutting down '{}'", m->name());
        m->shutdown();
    }

    modules_.clear();
}

// ----------------------------------------------------------------
// Queries
// ----------------------------------------------------------------

std::size_t ModuleManager::count() const {
    return modules_.size();
}

HealthStatus ModuleManager::health() const {
    if (modules_.empty()) {
        return HealthStatus::ok("no modules registered");
    }

    std::size_t ok = 0;
    std::size_t degraded = 0;
    std::size_t failed = 0;
    std::size_t unknown = 0;

    for (const auto& m : modules_) {
        const HealthStatus h = m->health();
        switch (h.level) {
            case HealthLevel::OK:       ++ok;       break;
            case HealthLevel::Degraded: ++degraded; break;
            case HealthLevel::Failed:   ++failed;   break;
            case HealthLevel::Unknown:  ++unknown;  break;
        }
    }

    const std::string summary =
        std::to_string(ok)       + " OK, " +
        std::to_string(degraded) + " degraded, " +
        std::to_string(failed)   + " failed, " +
        std::to_string(unknown)  + " unknown";

    if (failed > 0)   return {HealthLevel::Failed,   summary};
    if (degraded > 0) return {HealthLevel::Degraded, summary};
    if (unknown > 0)  return {HealthLevel::Unknown,  summary};
    return {HealthLevel::OK, summary};
}

} // namespace axiom::core