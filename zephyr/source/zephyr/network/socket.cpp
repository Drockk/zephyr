#include "zephyr/network/socket.hpp"

#include <stdexcept>
#include <utility>

#include <sys/socket.h>

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

Socket Socket::createTcp()
{
    const auto fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw std::runtime_error("Failed to create socket");
    }

    return Socket{fd};
}
}  // namespace zephyr::network
