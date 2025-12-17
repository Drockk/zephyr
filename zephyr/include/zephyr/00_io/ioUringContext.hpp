#pragma once

#include <atomic>
#include <cstdint>
#include <liburing.h>
#include <span>
#include <netinet/in.h>

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
    
    // UDP operations
    auto recvfrom(int32_t t_fd, std::span<std::byte> t_buffer, sockaddr_in& t_addr) -> ssize_t;
    auto sendto(int32_t t_fd, const std::span<const std::byte> t_buffer, const sockaddr_in& t_addr) -> ssize_t;

    // Cancel all pending operations
    auto cancel() -> void;
    
    // Check if cancelled
    auto is_cancelled() const -> bool { return m_cancelled.load(); }

private:
    io_uring m_ring{};
    std::atomic<bool> m_cancelled{false};
};
}
