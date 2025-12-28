#include <optional>
#include <span>

#include <zephyr/plugins/udpServer/details/protocol.hpp>
#include <zephyr/plugins/udpServer/UdpServerPlugin.hpp>
#include <zephyr/zephyr.hpp>

struct EchoController
{
    auto onMessage([[maybe_unused]] std::span<char> t_message) -> zephyr::plugins::udp::UdpProtocol::OutputType
    {
        return std::nullopt;
    }
};

using App = zephyr::core::Application<zephyr::plugins::udp::Plugin<EchoController, 5000>>;

int main()
{
    return zephyr::core::runApp<App>();
}
