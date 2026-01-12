#pragma once

#include "zephyr/plugin/details/pluginConcept.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

#include "exec/static_thread_pool.hpp"

namespace zephyr
{
template <plugin::PluginConcept... Plugins>
class Application
{
public:
    Application(std::string_view t_name, Plugins... t_plugins)
        : m_name{t_name},
          m_threadPool{std::thread::hardware_concurrency()},
          m_plugins{std::move(t_plugins)...}
    {}

    Application(std::string_view t_name, uint32_t t_threadNumber, Plugins... t_plugins)
        : m_name{t_name},
          m_threadPool{t_threadNumber},
          m_plugins{std::move(t_plugins)...}
    {}

    auto init() -> void
    {
        initPlugins();
    };

    auto run() -> void
    {
        auto work = runPlugins();
        stdexec::sync_wait(work);
    };

    auto stop() -> void
    {
        if (m_stopped) {
            return;
        }

        m_threadPool.request_stop();

        stopPlugins();

        m_stopped = true;
    };

private:
    auto initPlugins()
    {
        std::apply([](auto&... t_elements) { return ((t_elements.init()) && ...); }, m_plugins);
    }

    auto runPlugins()
    {
        return std::apply(
            [this](auto&... t_elements) {
                return stdexec::when_all(stdexec::on(m_threadPool.get_scheduler(), t_elements.run())...);
            },
            m_plugins);
    }

    auto stopPlugins()
    {
        std::apply([](auto&... t_elements) { return ((t_elements.stop()) && ...); }, m_plugins);
    }

    bool m_stopped{false};
    std::string m_name;
    exec::static_thread_pool m_threadPool;
    std::tuple<Plugins...> m_plugins;
};
}  // namespace zephyr
