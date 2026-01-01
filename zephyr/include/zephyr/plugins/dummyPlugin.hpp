#pragma once

#include <stdexec/execution.hpp>

#include <iostream>

namespace zephyr::plugins
{
struct DummyPlugin
{
    static auto init(stdexec::scheduler auto /*unused*/)
    {
        std::cout << "Init DummyPlugin\n";
    }

    static auto start()
    {
        std::cout << "Start DummyPlugin\n";
    }

    static auto stop()
    {
        std::cout << "Stop DummyPlugin\n";
    }
};
}  // namespace zephyr::plugins
