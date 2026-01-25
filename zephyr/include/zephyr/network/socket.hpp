#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <sys/types.h>

namespace zephyr::network
{
class Socket
{
public:
    Socket() = default;
    Socket(std::pair<const char*, uint16_t> t_endpoint);
    Socket(int32_t t_fd);

    auto create() -> void;
    auto setOptions(int32_t t_level, int32_t t_optname, int32_t t_value) const -> void;
    auto bind() const -> void;
    auto listen(int32_t t_backlog = 512) -> void;
    [[nodiscard]] auto accept() const -> int32_t;
    auto close() -> void;
    auto connect(std::pair<const char*, uint16_t> t_server) -> void;

    [[nodiscard]] auto receive(std::span<std::byte> t_buffer) const -> ssize_t;
    [[nodiscard]] auto send(std::span<std::byte> t_buffer) const -> ssize_t;

private:
    int32_t m_fd{-1};
    std::pair<const char*, uint16_t> m_endpoint{};
};
}  // namespace zephyr::network
