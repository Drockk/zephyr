#include "zephyr/network/socket.hpp"

#include <stdexcept>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace zephyr::network
{
Socket::Socket(std::pair<const char*, uint16_t> t_endpoint) : m_endpoint(t_endpoint) {}

auto Socket::create() -> void
{
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        throw std::runtime_error("Cannot create socket");
    }
}

auto Socket::setOptions(int32_t t_level, int32_t t_optname, int32_t t_value) const -> void
{
    setsockopt(m_fd, t_level, t_optname, &t_value, sizeof(t_value));
}

auto Socket::bind() const -> void
{
    const auto& [address, port] = m_endpoint;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(address);
    addr.sin_port = htons(port);

    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Cannot bind socket");
    }
}

auto Socket::listen(int32_t t_backlog) -> void
{
    if (::listen(m_fd, t_backlog) < 0) {
        close();
        throw std::runtime_error("Cannot listen on socket");
    }
}

auto Socket::close() -> void
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
}

auto Socket::receive(std::span<std::byte> t_buffer) const -> ssize_t
{
    return ::recv(m_fd, t_buffer.data(), t_buffer.size(), 0);
}

auto Socket::send(std::span<std::byte> t_buffer) const -> ssize_t
{
    return ::send(m_fd, t_buffer.data(), t_buffer.size(), 0);
}
}  // namespace zephyr::network