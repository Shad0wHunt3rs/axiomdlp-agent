#include "logger.h"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include "quill/sinks/RotatingFileSink.h"

#include <memory>
#include <stdexcept>

namespace axiom {

quill::Logger* Logger::logger_ = nullptr;

void Logger::init(const std::string& mode,
                  const std::string& log_file,
                  const std::string& log_level) {

    // Start the Quill backend thread
    quill::Backend::start();

    // Create the sink (output destination)
    std::shared_ptr<quill::Sink> sink;

    if (mode == "file") {
        // Use RotatingFileSinkConfig for rotation options
        quill::RotatingFileSinkConfig cfg;
        cfg.set_rotation_max_file_size(10 * 1024 * 1024); // 10 MB
        cfg.set_max_backup_files(5);
        cfg.set_open_mode('a');

        sink = quill::Frontend::create_or_get_sink<quill::RotatingFileSink>(
            log_file,
            [&cfg]() { return cfg; }()
        );
    } else {
        sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
    }

    // Create the logger
    // Note: the third argument is now a PatternFormatterOptions object
    quill::PatternFormatterOptions formatter_options{
        "%(time) [%(log_level)] [%(thread_id)] %(message)",
        "%Y-%m-%d %H:%M:%S.%Qns",
        quill::Timezone::LocalTime
    };

    logger_ = quill::Frontend::create_or_get_logger(
        "axiom",
        std::move(sink),
        formatter_options
    );

    // Set log level
    if (log_level == "debug") {
        logger_->set_log_level(quill::LogLevel::Debug);
    } else if (log_level == "warn") {
        logger_->set_log_level(quill::LogLevel::Warning);
    } else if (log_level == "error") {
        logger_->set_log_level(quill::LogLevel::Error);
    } else {
        logger_->set_log_level(quill::LogLevel::Info);
    }

    AXIOM_LOG_INFO("Logger initialized (mode={}, level={})", mode, log_level);
}

void Logger::shutdown() {
    if (logger_) {
        AXIOM_LOG_INFO("Logger shutting down...");
    }
    quill::Backend::stop();
    logger_ = nullptr;
}

quill::Logger* Logger::get() {
    if (!logger_) {
        throw std::runtime_error(
            "Logger not initialized. Call axiom::Logger::init() first.");
    }
    return logger_;
}

} // namespace axiom