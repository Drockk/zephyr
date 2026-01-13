#pragma once

#include <cstdint>

namespace zephyr::network
{
class Socket
{
public:
    Socket() = default;
    explicit Socket(int32_t t_fd);

    Socket(Socket&& t_other) noexcept;

    Socket& operator=(Socket&& t_other) noexcept;

    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    static Socket createTcp();

private:
    int32_t m_fd{};
};
}  // namespace zephyr::network
