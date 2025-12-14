#include <span>

#include <zephyr/plugins/udp/plugin.hpp>
#include <zephyr/zephyr.hpp>

struct EchoController
{
    auto onMessage([[maybe_unused]] std::span<char> t_message) {}
};

using App = zephyr::core::Application<zephyr::plugins::udp::Plugin<5000, EchoController>>;

int main()
{
    return zephyr::core::runApp<App>();
}
