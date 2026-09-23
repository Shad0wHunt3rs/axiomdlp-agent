// src/modules/example/example_module.cpp

#include "modules/example/example_module.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

bool ExampleModule::initialize(ServiceContext& context) {
    // Call the base class so ctx_ is stored.
    if (!ModuleBase::initialize(context)) {
        return false;
    }

    SPDLOG_INFO("example module: initializing");
    return true;
}

bool ExampleModule::start() {
    SPDLOG_INFO("example module: starting");
    return true;
}

void ExampleModule::stop() {
    SPDLOG_INFO("example module: stopping");
}

void ExampleModule::shutdown() {
    SPDLOG_INFO("example module: shutting down");
}

} // namespace axiom::core