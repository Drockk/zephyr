#include <zephyr/core/application.hpp>
#include <zephyr/core/entrypoint.hpp>
#include <zephyr/plugins/dummyPlugin.hpp>

using App = zephyr::core::Application<zephyr::plugins::DummyPlugin>;

int main()
{
    return zephyr::core::runApp<App>();
}
