#include "logger/logger.h"

int main() {
    axiom::Logger::init("console", "", "debug");

    AXIOM_LOG_DEBUG("This is a debug message");
    AXIOM_LOG_INFO("AxiomDLP Agent starting...");
    AXIOM_LOG_WARN("This is a warning: disk space low");
    AXIOM_LOG_ERROR("This is an error: failed to connect");
    AXIOM_LOG_INFO("Policy engine loaded {} policies", 42);

    axiom::Logger::shutdown();
    return 0;
}