// src/modules/module_base.h
//
// ModuleBase provides default implementations for the trivial methods of
// the IModule interface.

#pragma once

#include "core/health.h"
#include "core/service_context.h"
#include "interfaces/i_module.h"

#include <string>

namespace axiom::core {

class ModuleBase : public IModule {
public:
    ModuleBase() = default;
    ~ModuleBase() override = default;

    std::string version() const override {
        return "1.0.0";
    }

    bool initialize(ServiceContext& context) override {
        ctx_ = &context;
        return true;
    }

    bool start() override {
        return true;
    }

    void stop() override {
    }

    void shutdown() override {
    }

    HealthStatus health() const override {
        return HealthStatus::ok();
    }

protected:
    ServiceContext* ctx_ = nullptr;
};

} // namespace axiom::core
