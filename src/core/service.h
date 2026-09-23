// src/core/service.h
//
// Platform-agnostic service lifecycle interface.
//
// Linux:  implemented by LinuxService (systemd conventions)
// Windows: implemented by WindowsService (Service Control Manager)
//
// The agent's actual work (modules, event bus, etc.) is started inside
// run() and stopped before run() returns.

#pragma once

#include <memory>
#include <string>

namespace axiom::core {

// Forward declarations — we only pass pointers, so full definitions
// aren't needed in this header.
class ModuleManager;
struct ServiceContext;

class IService {
public:
    virtual ~IService() = default;

    // Run the service's main loop. Blocks until stop is requested.
    // Returns 0 on clean shutdown.
    virtual int run() = 0;

    // Ask the service to stop gracefully.
    virtual void request_stop() = 0;

    // Wire in dependencies before run() is called.
    virtual void set_module_manager(ModuleManager* mgr) = 0;
    virtual void set_service_context(ServiceContext* ctx) = 0;
};

// Platform-specific factory.
std::unique_ptr<IService> make_service(const std::string& service_name);

// Platform-specific install / uninstall.
// Return 0 on success, non-zero on error.
int install_service(const std::string& binary_path,
                    const std::string& service_name);

int uninstall_service(const std::string& service_name);

// The service name used by the platform (for systemd units / SCM).
std::string default_service_name();

} // namespace axiom::core