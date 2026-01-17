#include "zephyr/network/socket.hpp"

#include <stdexcept>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace zephyr::network
{
auto Socket::create() -> void
{
    auto m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        throw std::runtime_error("Cannot create socket");
    }
}

auto Socket::setOptions() -> void {}  // TODO

auto Socket::bind(std::pair<const char*, uint16_t> t_endpoint) -> void
{
    const auto& [address, port] = t_endpoint;

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
    ::close(m_fd);
    m_fd = -1;
}

auto Socket::receive() -> void {}

auto Socket::send() -> void {}
}  // namespace zephyr::network