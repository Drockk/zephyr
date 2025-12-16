#pragma once

#include "zephyr/core/logger.hpp"
#include "zephyr/core/plugin.hpp"

#include <exec/static_thread_pool.hpp>

#include <chrono>
#include <thread>
#include <tuple>

namespace zephyr::core
{

using namespace std::chrono_literals;

template <PluginConcept... Plugins>
class Application
{
public:
    Application() = default;

    Application(const Application&) = delete;
    Application(const Application&&) = delete;

    Application& operator=(Application&) = delete;
    Application& operator=(Application&&) = delete;

    ~Application()
    {
        stop();
    }

    auto init()
    {
        m_logger = Logger::createLogger("APP");

        ZEPHYR_LOG_INFO(m_logger, "Starting ZEPHYR Application");

        initPlugins();
    }

    auto start()
    {
        startPlugins();
        std::this_thread::sleep_for(5s);
    }

    auto stop()
    {
        stopPlugins();
        m_threadPool.request_stop();
    }

private:
    auto initPlugins()
    {
        std::apply([](auto&... t_elements) { return ((t_elements.init()) && ...); }, m_plugins);
    }

    auto startPlugins()
    {
        std::apply([scheduler = m_threadPool.get_scheduler()](
                       auto&... t_elements) { return ((t_elements.start(scheduler)) && ...); },
                   m_plugins);
    }

    auto stopPlugins()
    {
        std::apply([](auto&... t_elements) { return ((t_elements.stop()) && ...); }, m_plugins);
    }

    Logger::LoggerPtr m_logger;
    exec::static_thread_pool m_threadPool{std::thread::hardware_concurrency() - 1};  // - Logger thread
    std::tuple<Plugins...> m_plugins{};
};
}  // namespace zephyr::core