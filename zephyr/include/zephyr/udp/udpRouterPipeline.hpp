#pragma once

#include <memory>
#include <stdexec/execution.hpp>
#include <iostream>

#include "zephyr/context/context.hpp"
#include "zephyr/udp/udpProtocol.hpp"
#include "zephyr/udp/udpRouter.hpp"

namespace zephyr::udp
{
class UdpRouterPipeline
{
public:
    explicit UdpRouterPipeline(const UdpRouter& t_router)
        : m_router(t_router) {}

    auto operator()(UdpProtocol::InputType t_packet, std::shared_ptr<context::Context>) const
    {
        UdpPacket udpPacket;
        udpPacket.data = std::move(t_packet.data);
        udpPacket.source_ip = std::move(t_packet.source_ip);
        udpPacket.source_port = t_packet.source_port;
        udpPacket.dest_port = t_packet.dest_port;

        std::cout << "[UDP] Routing to port " << udpPacket.dest_port << "\n";

        return UdpProtocol::ResultSenderType{stdexec::just(m_router.route(udpPacket))};
    }

private:
    const UdpRouter& m_router;
};
}
