#pragma once

#include "zephyr/core/logger.hpp"

namespace zephyr::core
{
template <typename AppType>
int runApp()
{
    AppType app;

    Logger::init();

    app.init();
    app.start();
    app.stop();

    Logger::shutdown();

    return 0;
}
}  // namespace zephyr::core
