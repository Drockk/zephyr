#pragma once

#include "zephyr/core/plugin.hpp"

#include <exec/static_thread_pool.hpp>

#include <thread>
#include <tuple>

namespace zephyr::core
{
template <PluginConcept... Plugins>
class Application
{
public:
    Application() : m_threadPool(std::thread::hardware_concurrency()), m_scheduler(m_threadPool.get_scheduler()) {}

    Application(const Application&) = delete;
    Application(const Application&&) = delete;

    Application& operator=(Application&) = delete;
    Application& operator=(Application&&) = delete;

    ~Application()
    {
        m_threadPool.request_stop();
    }

    auto start()
    {
        initPlugins();
        startPlugins();
    }

private:
    auto initPlugins()
    {
        std::apply([](auto&... t_elements) { return ((t_elements.init()) && ...); }, m_plugins);
    }

    auto startPlugins()
    {
        std::apply([](auto&... t_elements) { return ((t_elements.start()) && ...); }, m_plugins);
    }

    exec::static_thread_pool m_threadPool;
    exec::static_thread_pool::scheduler m_scheduler;
    std::tuple<Plugins...> m_plugins{};
};
}  // namespace zephyr::core