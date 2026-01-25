#pragma once

#include "zephyr/network/socket.hpp"

#include <cstddef>
#include <span>

#include <sys/types.h>

namespace plugins::details
{
class TcpConnection
{
public:
    TcpConnection(zephyr::network::Socket&& t_socket);

    [[nodiscard]] auto receive(std::span<std::byte> t_buffer) const -> ssize_t;
    [[nodiscard]] auto send(std::span<std::byte> t_buffer) const -> ssize_t;

private:
    zephyr::network::Socket m_socket;
};
}  // namespace plugins::details
