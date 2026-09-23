// src/core/module_manager.h
//
// ModuleManager owns the collection of modules and drives their lifecycle.
//
// It handles:
//   - Registration (application wires modules in at startup)
//   - Initialization (passes ServiceContext to each module)
//   - Start / stop (in correct order)
//   - Shutdown (releases resources)
//   - Aggregate health reporting
//
// The manager does NOT know what any module does. It only knows the
// IModule interface. That is what makes the plugin architecture work.

#pragma once

#include "core/health.h"
#include "core/service_context.h"
#include "interfaces/i_module.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace axiom::core {

class ModuleManager {
public:
    ModuleManager() = default;
    ~ModuleManager() = default;

    // Non-copyable, non-movable: owns unique_ptr resources.
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;
    ModuleManager(ModuleManager&&) = delete;
    ModuleManager& operator=(ModuleManager&&) = delete;

    // Register a module. Ownership transfers to the manager.
    // Called by the Application at startup, before initialize_all().
    void register_module(std::unique_ptr<IModule> module);

    // Lifecycle — called in this order by the service.
    bool initialize_all(ServiceContext& context);
    bool start_all();
    void stop_all();
    void shutdown_all();

    // Queries
    std::size_t count() const;
    HealthStatus health() const;

private:
    std::vector<std::unique_ptr<IModule>> modules_;
};

} // namespace axiom::core
