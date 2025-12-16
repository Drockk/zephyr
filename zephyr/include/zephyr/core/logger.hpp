#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <spdlog/async_logger.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

// Zephyr convenience logging macros mapped to spdlog
#ifndef ZEPHYR_LOG_TRACE
#    define ZEPHYR_LOG_TRACE(logger, ...) SPDLOG_LOGGER_TRACE(logger, __VA_ARGS__)
#endif
#ifndef ZEPHYR_LOG_DEBUG
#    define ZEPHYR_LOG_DEBUG(logger, ...) SPDLOG_LOGGER_DEBUG(logger, __VA_ARGS__)
#endif
#ifndef ZEPHYR_LOG_INFO
#    define ZEPHYR_LOG_INFO(logger, ...) SPDLOG_LOGGER_INFO(logger, __VA_ARGS__)
#endif
#ifndef ZEPHYR_LOG_WARN
#    define ZEPHYR_LOG_WARN(logger, ...) SPDLOG_LOGGER_WARN(logger, __VA_ARGS__)
#endif
#ifndef ZEPHYR_LOG_ERROR
#    define ZEPHYR_LOG_ERROR(logger, ...) SPDLOG_LOGGER_ERROR(logger, __VA_ARGS__)
#endif
#ifndef ZEPHYR_LOG_CRITICAL
#    define ZEPHYR_LOG_CRITICAL(logger, ...) SPDLOG_LOGGER_CRITICAL(logger, __VA_ARGS__)
#endif

namespace zephyr::core
{
class Logger
{
public:
    using LoggerPtr = std::shared_ptr<spdlog::async_logger>;

    static auto init() -> void;
    static auto shutdown() -> void;

    static auto createLogger(std::string_view t_name) -> LoggerPtr;

private:
    static auto initSinks() -> void;

    inline static std::vector<spdlog::sink_ptr> m_sinks{};
};
}  // namespace zephyr::core
