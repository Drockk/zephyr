#pragma once

#include <exec/static_thread_pool.hpp>

#include <iostream>

namespace zephyr::plugins
{
struct DummyPlugin
{
    static auto init()
    {
        std::cout << "Init DummyPlugin\n";
    }

    static auto start(exec::static_thread_pool::scheduler)
    {
        std::cout << "Start DummyPlugin\n";
    }

    static auto stop()
    {
        std::cout << "Stop DummyPlugin\n";
    }
};
}  // namespace zephyr::plugins
