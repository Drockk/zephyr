#include <optional>
#include <span>

#include <zephyr/core/application.hpp>
#include <zephyr/network/addressV4.hpp>
#include <zephyr/network/endpoint.hpp>
#include <zephyr/plugins/udpServer/details/protocol.hpp>
#include <zephyr/plugins/udpServer/udpServer.hpp>
#include <zephyr/zephyr.hpp>

struct EchoController
{
    auto onMessage([[maybe_unused]] std::span<char> t_message) -> zephyr::plugins::udp::UdpProtocol::OutputType
    {
        return std::nullopt;
    }
};

int main()
{
    return zephyr::core::runApp(zephyr::core::Application{zephyr::plugins::UdpServer{
        zephyr::network::UdpEndpoint{zephyr::network::AddressV4::loopback(), 5000}, EchoController{}}});
}
