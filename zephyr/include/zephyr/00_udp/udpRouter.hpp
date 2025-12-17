#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <optional>
#include <vector>

#include "zephyr/context/context.hpp"
#include "zephyr/udp/udpPacket.hpp"

namespace zephyr::udp
{
class UdpRouter
{
public:
    using HandlerResult = std::optional<std::vector<uint8_t>>;
    using SyncHandler = std::function<HandlerResult(const UdpPacket&, const context::Context&)>;

    template<typename Handler>
    auto on_port(uint16_t t_port, Handler&& t_handler)
    {
        m_routes.emplace_back(t_port, std::forward<Handler>(t_handler));
    }

    auto route(UdpPacket& t_packet) const
        -> HandlerResult
    {
        for (const auto& [port, handler]: m_routes) {
            if (port == t_packet.dest_port) {
                return handler(t_packet, *m_context);
            }
        }

        return std::nullopt;
    }

    const auto& context() const
    {
        return *m_context;
    }

private:
    struct Route
    {
        uint16_t port;
        SyncHandler handler;
    };

    std::vector<Route> m_routes;
    std::shared_ptr<context::Context> m_context{ std::make_shared<context::Context>() };
};
}
