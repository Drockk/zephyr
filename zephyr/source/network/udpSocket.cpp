#include "zephyr/network/udpSocket.hpp"

#include "zephyr/core/logger.hpp"
#include "zephyr/network/endpoint.hpp"

#include <cerrno>
#include <cstring>
#include <format>
#include <stdexcept>
#include <unistd.h>

#include <sys/socket.h>

namespace zephyr::network
{
UdpSocket::UdpSocket(core::Logger::LoggerPtr& t_logger, UdpEndpoint t_endpoint) noexcept
    : m_logger(t_logger),
      m_endpoint(t_endpoint),
      m_socket(-1)
{}

UdpSocket::~UdpSocket() noexcept
{
    close();
}

auto UdpSocket::bind() -> void
{
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        throw std::runtime_error(std::format("Cannot create socket. Error({}): {}", errno, std::strerror(errno)));
    }

    const auto [address, addressLength] = m_endpoint.toSockaddr();
    if (::bind(m_socket, reinterpret_cast<const sockaddr*>(&address), addressLength) < 0) {
        ::close(m_socket);
        m_socket = -1;
        throw std::runtime_error(std::format("Cannot bind udp socket. Error({}): {}", errno, std::strerror(errno)));
    }

    ZEPHYR_LOG_INFO(m_logger, "Bound {}", m_endpoint);
}

auto UdpSocket::close() noexcept -> void
{
    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
    }
}
}  // namespace zephyr::network
