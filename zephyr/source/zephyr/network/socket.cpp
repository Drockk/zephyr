#include "zephyr/network/socket.hpp"

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <optional>
#include <span>
#include <stdexcept>
#include <unistd.h>
#include <utility>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace zephyr::network
{
Socket::Socket(int32_t t_fd) : m_fd{t_fd} {}

Socket::Socket(Socket&& t_other) noexcept : m_fd{t_other.m_fd}
{
    t_other.m_fd = -1;
}

Socket& Socket::operator=(Socket&& t_other) noexcept
{
    if (this != &t_other) {
        close();
        m_fd = std::exchange(t_other.m_fd, -1);
    }

    return *this;
}

Socket::~Socket()
{
    close();
}

auto Socket::createTcp() -> Socket
{
    const auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw std::runtime_error("Failed to create socket");
    }

    return Socket{fd};
}

auto Socket::setReuseAddress() const noexcept -> void
{
    const auto opt = 1;
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

auto Socket::setNonBlocking() const noexcept -> void
{
    const auto flags = fcntl(m_fd, F_GETFL, 0);
    fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
}

auto Socket::bind(const in_addr_t t_address, const uint16_t t_port) const -> void
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(t_port);
    address.sin_addr.s_addr = t_address;

    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
        throw std::runtime_error("Failed to bind");
    }
}

auto Socket::listen(int32_t t_backlog) const -> void
{
    ::listen(m_fd, t_backlog);
}

auto Socket::accept() const -> std::optional<Socket>
{
    const auto client = ::accept(m_fd, nullptr, nullptr);
    if (client == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::nullopt;
        }

        throw std::runtime_error("Accept failed");
    }

    return Socket(client);
}

auto Socket::recv(std::span<uint8_t> t_buffer) const noexcept -> ssize_t
{
    return ::recv(m_fd, t_buffer.data(), t_buffer.size(), 0);
}

auto Socket::send(const std::span<uint8_t> t_buffer) const noexcept -> ssize_t
{
    return ::send(m_fd, t_buffer.data(), t_buffer.size(), MSG_NOSIGNAL);
}

auto Socket::close() -> void
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
}

[[nodiscard]] auto Socket::fd() const noexcept -> int
{
    return m_fd;
}

[[nodiscard]] auto Socket::isValid() const noexcept -> bool
{
    return m_fd != -1;
}
}  // namespace zephyr::network
