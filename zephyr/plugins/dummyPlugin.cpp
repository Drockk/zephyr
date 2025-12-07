module;

#include <iostream>

export module zephyr.dummyPlugin;

import zephyr.plugin;

namespace zephyr
{
export struct DummyPlugin
{
    auto init()
    {
        std::cout << "Init DummyPlugin\n";
        return Result::ok;
    }

    auto start()
    {
        std::cout << "Start DummyPlugin\n";
        return Result::ok;
    }
};
}
