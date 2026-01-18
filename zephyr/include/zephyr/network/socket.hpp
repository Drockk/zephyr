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
    Socket(std::pair<const char*, uint16_t> t_endpoint);

    auto create() -> void;
    auto setOptions(int32_t t_level, int32_t t_optname, int32_t t_value) const -> void;
    auto bind() const -> void;
    auto listen(int32_t t_backlog = 512) -> void;
    auto close() -> void;
    auto receive(std::span<std::byte> t_buffer) const -> ssize_t;
    auto send(std::span<std::byte> t_buffer) const -> ssize_t;

private:
    int32_t m_fd{-1};
    std::pair<const char*, uint16_t> m_endpoint;
};
}  // namespace zephyr::network
