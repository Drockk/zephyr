#include "zephyr/network/udpSocket.hpp"

#include <sys/socket.h>

namespace zephyr::network
{
auto UdpSocket::bind() -> void
{
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
}
}  // namespace zephyr::network
