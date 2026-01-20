#include "plugins/tcpServer/details/tcpConnection.hpp"

#include <utility>

namespace plugins::details
{
TcpConnection::TcpConnection(zephyr::network::Socket t_socket) : m_socket(std::move(t_socket)) {}

auto TcpConnection::receive(std::span<std::byte> t_buffer) const -> ssize_t
{
    return m_socket.receive(t_buffer);
}

auto TcpConnection::send(std::span<std::byte> t_buffer) const -> ssize_t
{
    return m_socket.send(t_buffer);
}
}  // namespace plugins::details
