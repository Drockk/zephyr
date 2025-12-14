#pragma once

#include "zephyr/core/plugin.hpp"

#include <iostream>

namespace zephyr::plugins
{
struct DummyPlugin
{
    static auto init()
    {
        std::cout << "Init DummyPlugin\n";
        return core::Result::OK;
    }

    static auto start()
    {
        std::cout << "Start DummyPlugin\n";
        return core::Result::OK;
    }
};
}  // namespace zephyr::plugins
