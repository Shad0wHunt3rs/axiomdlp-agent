// src/core/logger.cpp

#include "core/logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <filesystem>
#include <vector>

namespace axiom::core {

void Logger::init(const std::string& log_dir,
                  const std::string& log_file,
                  const std::string& level) {

    // Ensure the log directory exists.
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    const std::string log_path = log_dir + "/" + log_file;

    // Async thread pool: 8192-slot queue, 1 background worker thread.
    spdlog::init_thread_pool(8192, 1);

    // Console sink (colored output for terminals).
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    // Rotating file sink: 5 MB per file, keep 3 rotated files (total ~20 MB cap).
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path, 1024 * 1024 * 5, 3);
    file_sink->set_level(spdlog::level::trace);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

    // Async logger — never blocks the caller.
    // overrun_oldest: if the queue is full, drop the oldest message rather than block.
    auto logger = std::make_shared<spdlog::async_logger>(
        "axiom",
        sinks.begin(), sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    // Format: [timestamp] [level] [thread-id] message
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [t:%t] %v");

    // Set level from config string.
    logger->set_level(spdlog::level::from_str(level));

    // Flush immediately on warn and above (so important events survive crashes).
    logger->flush_on(spdlog::level::warn);

    // Register as the global default logger.
    spdlog::set_default_logger(logger);
}

void Logger::shutdown() {
    spdlog::shutdown();
}

} // namespace axiom::core