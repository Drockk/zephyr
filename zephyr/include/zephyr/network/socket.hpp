#pragma once

#include <cstdint>
#include <utility>

namespace zephyr::network
{
class Socket
{
public:
    auto create() -> void;
    auto setOptions() -> void;
    auto bind(std::pair<const char*, uint16_t> t_endpoint) -> void;
    auto listen(int32_t t_backlog = 512) -> void;
    auto close() -> void;
    auto receive() -> void;
    auto send() -> void;

private:
    int32_t m_fd{-1};
};
}  // namespace zephyr::network
