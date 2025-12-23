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

constexpr auto udpAddress = zephyr::network::AddressV4{};
constexpr uint16_t udpPort = 5000;

using App = zephyr::core::Application<zephyr::plugins::udp::Plugin<udpPort, udpAddress, EchoController>>;

int main()
{
    return zephyr::core::runApp<App>();
}
