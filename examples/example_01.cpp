#include <zephyr/core/application.hpp>
#include <zephyr/plugins/dummyPlugin.hpp>

using App = zephyr::core::Application<
    zephyr::plugins::DummyPlugin
>;

int main()
{
    App app;

    app.start();

    return 0;
}
