#pragma once

#include "zephyr/core/logger.hpp"
#include "zephyr/core/pluginConcept.hpp"

#include <exec/linux/io_uring_context.hpp>
#include <exec/static_thread_pool.hpp>

#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace zephyr::core
{

using namespace std::chrono_literals;

template <PluginConcept... Plugins>
class Application
{
public:
    Application()
        requires(std::is_default_constructible_v<Plugins> && ...)
    = default;

    template <typename... PluginArgs>
        requires(sizeof...(PluginArgs) == sizeof...(Plugins))
    explicit Application(PluginArgs&&... plugins) : m_plugins(std::forward<PluginArgs>(plugins)...)
    {}

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

        std::jthread loop([this] { stdexec::sync_wait(m_context.run(exec::until::stopped)); });
        if (loop.joinable()) {
            loop.join();
        }
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
        std::apply([scheduler = m_context.get_scheduler()](
                       auto&... t_elements) { return ((t_elements.start(scheduler)) && ...); },
                   m_plugins);
    }

    auto stopPlugins()
    {
        std::apply([](auto&... t_elements) { return ((t_elements.stop()) && ...); }, m_plugins);
    }

    Logger::LoggerPtr m_logger;
    exec::static_thread_pool m_threadPool{std::thread::hardware_concurrency()
                                          - 2};  // - Logger thread - io_uring thread
    std::tuple<Plugins...> m_plugins{};
    exec::io_uring_context m_context{1024};
};

template <typename... PluginArgs>
Application(PluginArgs&&...) -> Application<std::remove_cvref_t<PluginArgs>...>;

}  // namespace zephyr::core