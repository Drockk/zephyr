#pragma once

namespace zephyr::core
{
template <typename AppType>
int runApp()
{
    AppType app;

    app.init();
    app.start();
    app.stop();

    return 0;
}
}  // namespace zephyr::core
