#pragma once

#include <cstdint>
#include <liburing.h>
#include <span>

namespace zephyr::io
{
class IoUringContext
{
public:
    explicit IoUringContext(uint32_t t_entries = 256);
    ~IoUringContext();

    auto accept(int32_t t_listen_fd) -> int;
    auto receive(int32_t t_fd, std::span<std::byte> t_buffer) -> ssize_t;
    auto send(int32_t t_fd, const std::span<const std::byte> t_buffer) -> ssize_t;

private:
    io_uring m_ring{};
};
}
