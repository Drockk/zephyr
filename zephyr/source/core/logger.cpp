#include "zephyr/core/logger.hpp"

#include <memory>
#include <string_view>
#include <vector>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/cfg/env.h>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/spdlog.h>
#include <sys/syslog.h>

namespace zephyr::core
{
auto Logger::init() -> void
{
    spdlog::cfg::load_env_levels("ZEPHYR_LOG_LEVEL");
    spdlog::init_thread_pool(8192, 1);

    initSinks();
}

auto Logger::shutdown() -> void
{
    spdlog::shutdown();
}

auto Logger::initSinks() -> void
{
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::debug);
    consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%s:%#] %v");

    auto syslogSink = std::make_shared<spdlog::sinks::syslog_sink_mt>("ZEPHYR", LOG_PID, LOG_USER, false);
    syslogSink->set_level(spdlog::level::info);
    syslogSink->set_pattern("[%n] [%l] [%s:%#] %v");

    m_sinks.emplace_back(consoleSink);
    m_sinks.emplace_back(syslogSink);
}

auto Logger::createLogger(std::string_view t_name) -> LoggerPtr
{
    auto tp = spdlog::thread_pool();
    if (!tp) {
        spdlog::init_thread_pool(8192, 1);
        tp = spdlog::thread_pool();
    }

    auto logger = std::make_shared<spdlog::async_logger>(std::string{t_name}, m_sinks.begin(), m_sinks.end(),
                                                         std::weak_ptr<spdlog::details::thread_pool>{tp},
                                                         spdlog::async_overflow_policy::overrun_oldest);

    spdlog::register_logger(logger);

    return logger;
}
}  // namespace zephyr::core