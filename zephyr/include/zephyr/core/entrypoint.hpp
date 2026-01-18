#pragma once

#ifdef NDEBUG
#    include <exception>
#    include <iostream>
#endif

namespace zephyr
{
auto runApplication(auto&& t_app)
{
#ifdef NDEBUG
    try {
#endif
        t_app.init();
        t_app.run();
        t_app.stop();
#ifdef NDEBUG
    } catch (std::exception& t_exception) {
        std::cerr << t_exception.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Uknown exception\n";
        return 1;
    }
#endif

    return 0;
}
}  // namespace zephyr
