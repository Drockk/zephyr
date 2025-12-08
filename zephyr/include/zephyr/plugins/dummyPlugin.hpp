#pragma once

#include <iostream>

#include "zephyr/core/plugin.hpp"

namespace zephyr::plugins
{
struct DummyPlugin
{
    auto init()
    {
        std::cout << "Init DummyPlugin\n";
        return core::Result::ok;
    }

    auto start()
    {
        std::cout << "Start DummyPlugin\n";
        return core::Result::ok;
    }
};
}
