// src/core/service.h
//
// Platform-agnostic service lifecycle interface.
//
// Linux:  implemented by LinuxService (systemd conventions)
// Windows: implemented by WindowsService (Service Control Manager)
//
// The service drives the module lifecycle. It does not need the
// ServiceContext — that is for modules only. The service only needs
// the ModuleManager.

#pragma once

#include <memory>
#include <string>

namespace axiom::core {

// Forward declaration — we only pass a pointer, so the full definition
// isn't needed in this header.
class ModuleManager;

class IService {
public:
    virtual ~IService() = default;

    // Run the service's main loop. Blocks until stop is requested.
    // Returns 0 on clean shutdown.
    virtual int run() = 0;

    // Ask the service to stop gracefully.
    virtual void request_stop() = 0;

    // Wire in the module manager before run() is called.
    virtual void set_module_manager(ModuleManager* mgr) = 0;
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