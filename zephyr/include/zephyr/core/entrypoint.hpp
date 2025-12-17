#pragma once

#include "zephyr/core/logger.hpp"

#ifdef NDEBUG
#    include <iostream>
#endif

namespace zephyr::core
{
template <typename AppType>
int runApp()
{
#ifdef NDEBUG
    try {
#endif
        AppType app;

        Logger::init();

        app.init();
        app.start();
        app.stop();

        Logger::shutdown();

#ifdef NDEBUG
    } catch (const std::exception& t_exception) {
        std::cerr << t_exception.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Uknown exception\n";
        return 1;
    }
#endif

    return 0;
}
}  // namespace zephyr::core
