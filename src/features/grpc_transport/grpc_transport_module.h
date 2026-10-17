// src/features/grpc_transport/grpc_transport_module.h
//
// gRPC transport module. Only compiled when AXIOM_HAVE_GRPC is defined
// (currently: Windows + vcpkg + MSVC). On Linux the module is excluded
// until scripts/linux/build-grpc.sh is run and the CMake block is
// extended to pick up .deps/grpc-linux-*/.
//
// The module is **additive** — WebSocket transport stays in place.
// On Windows you can run both at once (gRPC disabled by default in
// config, enabled with modules.grpc_transport.enabled = true).

#pragma once

#ifdef AXIOM_HAVE_GRPC

#include "modules/module_base.h"

#include <atomic>
#include <string>
#include <thread>

namespace axiom::core {

class GrpcTransportModule final : public ModuleBase {
public:
    GrpcTransportModule();
    ~GrpcTransportModule() override;

    std::string name() const override { return "grpc_transport"; }

    bool initialize(ModuleContext& context) override;
    bool start() override;
    void stop() override;
    void shutdown() override;
    HealthStatus health() const override;

private:
    void manager_loop();

    std::string server_url_;
    std::string api_token_;
    int         agent_id_ = 0;

    std::thread       manager_thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
};

} // namespace axiom::core

#endif // AXIOM_HAVE_GRPC
