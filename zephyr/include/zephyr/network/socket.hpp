#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

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

    static auto createTcp() -> Socket;

    auto setReuseAddress() const noexcept -> void;

    auto setNonBlocking() const noexcept -> void;

    auto bind(in_addr_t t_address, uint16_t t_port) const -> void;

    auto listen(int32_t t_backlog = SOMAXCONN) const -> void;

    [[nodiscard]] auto accept() const -> std::optional<Socket>;

    [[nodiscard]] auto recv(std::span<uint8_t> t_buffer) const noexcept -> ssize_t;

    [[nodiscard]] auto send(std::span<uint8_t> t_buffer) const noexcept -> ssize_t;

    auto close() -> void;

    [[nodiscard]] auto fd() const noexcept -> int;

    [[nodiscard]] auto isValid() const noexcept -> bool;

private:
    int32_t m_fd{-1};
};
}  // namespace zephyr::network
