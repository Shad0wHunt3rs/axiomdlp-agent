// src/core/service_context.h

#pragma once

#include "core/config.h"

namespace axiom::core {

struct ServiceContext {
    const Config& config;

    explicit ServiceContext(const Config& c) : config(c) {}
};

} // namespace axiom::core