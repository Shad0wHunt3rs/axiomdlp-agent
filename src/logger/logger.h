#pragma once

#include <string>

#include "quill/LogMacros.h"
#include "quill/Logger.h"

namespace axiom {

class Logger {
public:
    static void init(const std::string& mode = "console",
                     const std::string& log_file = "axiomdlp.log",
                     const std::string& log_level = "info");

    static void shutdown();

    static quill::Logger* get();

private:
    static quill::Logger* logger_;
};

} // namespace axiom

// ============================================================================
// LOGGING MACROS
// ============================================================================

#define AXIOM_LOG_DEBUG(fmt, ...)    QUILL_LOG_DEBUG(axiom::Logger::get(), fmt, ##__VA_ARGS__)
#define AXIOM_LOG_INFO(fmt, ...)     QUILL_LOG_INFO(axiom::Logger::get(), fmt, ##__VA_ARGS__)
#define AXIOM_LOG_WARN(fmt, ...)     QUILL_LOG_WARNING(axiom::Logger::get(), fmt, ##__VA_ARGS__)
#define AXIOM_LOG_ERROR(fmt, ...)    QUILL_LOG_ERROR(axiom::Logger::get(), fmt, ##__VA_ARGS__)
#define AXIOM_LOG_CRITICAL(fmt, ...) QUILL_LOG_CRITICAL(axiom::Logger::get(), fmt, ##__VA_ARGS__)