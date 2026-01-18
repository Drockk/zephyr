#pragma once

#include "zephyr/network/socket.hpp"

namespace plugins::details
{
class TcpAcceptor
{
public:
    TcpAcceptor(zephyr::network::Socket&& t_socket);

    auto init() -> void;
    auto acceptConnectins() -> void;
    auto stop() -> void;

private:
    zephyr::network::Socket m_socket;
};
}  // namespace plugins::details