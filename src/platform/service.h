// src/platform/service.h
//
// Platform-agnostic service lifecycle interface.
//
// Linux:  systemd conventions
// Windows: Service Control Manager
//
// The service drives the module lifecycle. It does NOT own the
// ShutdownController — that lives in Application and is shared with
// every module through ModuleContext.

#pragma once

#include "foundation/shutdown.h"

#include <memory>
#include <string>

namespace axiom::core {

class ModuleManager;

class IService {
public:
    virtual ~IService() = default;

    // Run the service's main loop. Blocks until a shutdown is
    // requested via ShutdownController. Returns 0 on clean shutdown.
    virtual int run() = 0;

    // Request a graceful shutdown with a reason. Idempotent: only the
    // first call has any effect.
    virtual void request_stop(ShutdownReason reason) = 0;

    // Wire in the module manager and the shared shutdown controller
    // before run() is called.
    virtual void set_module_manager(ModuleManager* mgr) = 0;
    virtual void set_shutdown_controller(ShutdownController* sc) = 0;
};

// Platform-specific factory.
std::unique_ptr<IService> make_service(const std::string& service_name);

// Platform-specific install / uninstall.
int install_service(const std::string& binary_path,
                    const std::string& service_name);
int uninstall_service(const std::string& service_name);

std::string default_service_name();

} // namespace axiom::core