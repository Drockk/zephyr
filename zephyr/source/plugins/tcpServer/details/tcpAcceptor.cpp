#include "plugins/tcpServer/details/tcpAcceptor.hpp"

#include <arpa/inet.h>

namespace plugins::details
{
TcpAcceptor::TcpAcceptor(zephyr::network::Socket&& t_socket) : m_socket(std::move(t_socket)) {}

auto TcpAcceptor::init() -> void
{
    m_socket.create();
    m_socket.setOptions(SOL_SOCKET, SO_REUSEADDR, 1);
    m_socket.bind();
    m_socket.listen();
}

auto acceptConnectins() -> void {}

auto TcpAcceptor::stop() -> void
{
    m_socket.close();
}
}  // namespace plugins::details
