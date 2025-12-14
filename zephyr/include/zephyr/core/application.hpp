#pragma once

#include "zephyr/core/plugin.hpp"

#include <exec/static_thread_pool.hpp>

#include <exception>
#include <iostream>
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
        const auto result =
            std::apply([](auto&... t_elements) { return ((t_elements.init() == Result::OK) && ...); }, m_plugins);

        if (!result) {
            std::cerr << "Initialization failed\n";
            std::terminate();
        }
    }

    auto startPlugins()
    {
        const auto result =
            std::apply([](auto&... t_elements) { return ((t_elements.start() == Result::OK) && ...); }, m_plugins);

        if (!result) {
            std::cerr << "Start failed\n";
            std::terminate();
        }
    }

    exec::static_thread_pool m_threadPool;
    exec::static_thread_pool::scheduler m_scheduler;
    std::tuple<Plugins...> m_plugins{};
};
}  // namespace zephyr::core