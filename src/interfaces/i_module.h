// src/interfaces/i_module.h
//
// The module interface. Every feature in the agent implements this
// contract. The ModuleManager drives the lifecycle uniformly.
//
// Lifecycle:
//   1. initialize(context)  — called once, before start
//   2. start()              — called once after all modules initialized
//   3. stop()               — called on shutdown
//   4. shutdown()           — called once, after stop, to release resources
//
// Modules must be self-contained. They cannot call each other directly.
// Communication between modules happens through the event bus (Milestone 6).

#pragma once

#include "core/health.h"
#include "core/service_context.h"

#include <string>

namespace axiom::core {

class IModule {
public:
    virtual ~IModule() = default;

    // Identity ------------------------------------------------------

    // A short, unique identifier. Lowercase, no spaces.
    // Examples: "filesystem", "usb", "clipboard".
    virtual std::string name() const = 0;

    // A semantic version string (e.g., "1.0.0").
    virtual std::string version() const = 0;

    // Lifecycle -----------------------------------------------------

    // Called once at agent startup, before start().
    // The module receives the shared ServiceContext here.
    // Returns true on success, false on failure.
    virtual bool initialize(ServiceContext& context) = 0;

    // Called after all modules have been initialized.
    // The module should begin its work here (spawn threads, register
    // callbacks, etc.). Returns true on success, false on failure.
    virtual bool start() = 0;

    // Called during agent shutdown. The module should stop its work.
    // Stop must be idempotent — safe to call multiple times.
    virtual void stop() = 0;

    // Called once after stop(), to release resources.
    // After shutdown, the module is not expected to start again.
    virtual void shutdown() = 0;

    // Reporting -----------------------------------------------------

    // Report the module's current health.
    virtual HealthStatus health() const = 0;
};

} // namespace axiom::core
