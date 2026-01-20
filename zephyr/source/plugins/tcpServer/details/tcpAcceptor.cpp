#include "plugins/tcpServer/details/tcpAcceptor.hpp"

#include <utility>

#include <arpa/inet.h>
#include <asm-generic/socket.h>

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

auto TcpAcceptor::stop() -> void
{
    m_socket.close();
    m_pool->request_stop();
}
}  // namespace plugins::details
