#pragma once

#include <tuple>
#include <iostream>
#include "zephyr/core/plugin.hpp"

namespace zephyr::core
{
template<PluginConcept... Plugins>
class Application
{
public:
    auto start()
    {
        init_plugins();
        start_plugins();
    }

private:
    auto init_plugins()
    {
        const auto ok = std::apply([](auto&... t_elements) {
            return ((t_elements.init() == Result::ok) && ...);
        }, m_plugins);

        if (!ok)
        {
            std::cerr << "Initialization failed\n";
            std::terminate();
        }
    }

    auto start_plugins()
    {
        const bool ok = std::apply([](auto&... t_elements) {
            return ((t_elements.start() == Result::ok) && ...);
        }, m_plugins);

        if (!ok)
        {
            std::cerr << "Start failed\n";
            std::terminate();
        }
    }

    std::tuple<Plugins...> m_plugins{};
};
}