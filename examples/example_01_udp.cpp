#include <optional>
#include <span>

#include <zephyr/plugins/udp/details/protocol.hpp>
#include <zephyr/plugins/udp/plugin.hpp>
#include <zephyr/zephyr.hpp>

struct EchoController
{
    auto onMessage([[maybe_unused]] std::span<char> t_message) -> zephyr::plugins::udp::UdpProtocol::OutputType
    {
        return std::nullopt;
    }
};

using App = zephyr::core::Application<zephyr::plugins::udp::Plugin<5000, EchoController>>;

int main()
{
    return zephyr::core::runApp<App>();
}
