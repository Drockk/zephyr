#pragma once

#include "zephyr/core/logger.hpp"

#include <utility>

#ifdef NDEBUG
#    include <iostream>
#endif

namespace zephyr::core
{
template <typename AppType>
auto runApp(AppType&& t_app)
{
#ifdef NDEBUG
    try {
#endif
        Logger::init();

        t_app.init();
        t_app.start();
        t_app.stop();

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

template <typename AppType>
    requires std::is_default_constructible_v<AppType>
auto runApp()
{
    AppType app{};
    return runApp(std::move(app));
}
}  // namespace zephyr::core
