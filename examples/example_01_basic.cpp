#include <zephyr/core/application.hpp>
#include <zephyr/plugins/dummyPlugin.hpp>

using App = zephyr::core::Application<zephyr::plugins::DummyPlugin>;

int main()
{
    App app;

    app.init();
    app.start();
    app.stop();

    return 0;
}
