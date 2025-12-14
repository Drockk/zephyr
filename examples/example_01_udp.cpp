#include <span>

#include <zephyr/core/application.hpp>
#include <zephyr/plugins/udp/plugin.hpp>

struct EchoController
{
    auto onMessage([[maybe_unused]] std::span<char> t_message) {}
};

using App = zephyr::core::Application<zephyr::plugins::udp::Plugin<5000, EchoController>>;

int main()
{
    App app;

    app.start();

    return 0;
}
