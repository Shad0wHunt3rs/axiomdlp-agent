// src/core/service.h
//
// Platform-agnostic service lifecycle interface.
//
// One implementation per platform (Linux systemd / Windows SCM),
// selected at build time by CMake.

#pragma once

#include <memory>
#include <string>

namespace axiom::core {

class IService {
public:
    virtual ~IService() = default;

    // Run the service main loop. Blocks until stop is requested.
    // Returns 0 on clean shutdown.
    virtual int run() = 0;

    // Ask the service to stop gracefully.
    virtual void request_stop() = 0;
};

// Factory — returns the platform-specific service implementation.
std::unique_ptr<IService> make_service(const std::string& service_name);

// Platform-specific install / uninstall.
// Return 0 on success, non-zero on error.
int install_service(const std::string& binary_path,
                    const std::string& service_name);

int uninstall_service(const std::string& service_name);

// The service name used by the platform (for systemd units / SCM).
std::string default_service_name();

} // namespace axiom::core