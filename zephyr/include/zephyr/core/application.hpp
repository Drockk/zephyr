#pragma once

#include "zephyr/core/details/signalHandler.hpp"
#include "zephyr/plugin/details/pluginConcept.hpp"

#include <exec/static_thread_pool.hpp>

#include <csignal>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

namespace zephyr
{
template <plugin::PluginConcept... Plugins>
class Application
{
public:
    Application(std::string_view t_name, Plugins&&... t_plugins)
        : m_name{t_name},
          m_threadPool{std::thread::hardware_concurrency()},
          m_plugins{std::forward<Plugins>(t_plugins)...}
    {}

    Application(std::string_view t_name, uint32_t t_threadNumber, Plugins&&... t_plugins)
        : m_name{t_name},
          m_threadPool{t_threadNumber},
          m_plugins{std::forward<Plugins>(t_plugins)...}
    {}

    auto init() -> void
    {
        initPlugins();
    };

    auto run() -> void
    {
        runPlugins();

        details::SignalHandler::instance().wait();
    };

    auto stop() -> void
    {
        if (m_stopped.exchange(true)) {
            return;
        }

        stopPlugins();
        m_threadPool.request_stop();
    };

private:
    auto setupSignalHandlers()
    {
        details::SignalHandler::instance().reset();
        std::signal(SIGINT, details::signalCallback);
        std::signal(SIGTERM, details::signalCallback);
    }

    auto initPlugins()
    {
        std::apply([](auto&... t_elements) { ((t_elements.init()) && ...); }, m_plugins);
    }

    auto runPlugins()
    {
        std::apply([this](auto&... t_elements) { ((t_elements.run(m_threadPool.get_scheduler())) && ...); }, m_plugins);

        while (m_stopped) {}
    }

    auto stopPlugins()
    {
        std::apply([](auto&... t_elements) { ((t_elements.stop()) && ...); }, m_plugins);
    }

    std::atomic<bool> m_stopped{false};
    std::string m_name;
    exec::static_thread_pool m_threadPool;
    std::tuple<Plugins...> m_plugins;
};
}  // namespace zephyr
