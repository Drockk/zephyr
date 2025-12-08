#pragma once

#include <cstring>
#include <functional>
#include <liburing.h>
#include <map>

namespace zephyr::io
{
class IoUringContext
{
public:
    IoUringContext();

    ~IoUringContext();

    auto run() -> void;

    auto stop() -> void;

    auto async_accept(int t_listen_fd, sockaddr* t_address, socklen_t* t_address_length,
                        std::function<void(int)> t_handler) -> unsigned long;

    auto async_read(int t_fd, void* t_buffer, size_t t_length,
                    std::function<void(int)> t_handler) -> unsigned long;

    auto async_write(int t_fd, const void* t_buffer, size_t t_length,
                    std::function<void(int)> t_handler) -> unsigned long;

    auto async_close(int t_fd, std::function<void(int)> t_handler) -> unsigned long;

private:
    io_uring m_ring;
    bool m_running{ false };

    std::map<uint64_t, std::function<void(int)>> m_pending_operations;
    uint64_t m_next_operation_id{ 1 };
};
}
