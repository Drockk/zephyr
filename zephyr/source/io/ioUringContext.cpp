#include "zephyr/io/ioUringContext.hpp"

#include <format>
#include <iostream>
#include <stdexcept>

namespace zephyr::io
{
IoUringContext::IoUringContext()
{
    const auto result = io_uring_queue_init(256, &m_ring, 0);
    if (result < 0) {
        throw std::runtime_error("Failed to initialize io_uring");
    }
}

IoUringContext::~IoUringContext()
{
    io_uring_queue_exit(&m_ring);
}

auto IoUringContext::run() -> void
{
    m_running = true;

    while (m_running) {
        io_uring_cqe* cqe{ nullptr };

        auto result = io_uring_wait_cqe(&m_ring, &cqe);
        if (result < 0) {
            std::cerr << std::format("Error waiting for completion: {}\n", strerror(-result));
            break;
        }

        uint32_t head{};
        uint32_t count{ 0 };

        io_uring_for_each_cqe(&m_ring, head, cqe) {
            ++count;

            auto operation_id = cqe->user_data;
            auto result = cqe->res;

            if(m_pending_operations.contains(operation_id)) {
                m_pending_operations.at(operation_id)(result);
                m_pending_operations.erase(operation_id);
            }
            else {
                std::cerr << std::format("No handler for operation ID: {}\n", operation_id);
            }
        }

        io_uring_cq_advance(&m_ring, count);
    }
}

auto IoUringContext::stop() -> void
{
    m_running = false;
}

auto IoUringContext::async_accept(int t_listen_fd, sockaddr* t_address, socklen_t* t_address_length,
                    std::function<void(int)> t_handler) -> unsigned long
{
    const auto operation_id = m_next_operation_id++;
    m_pending_operations[operation_id] = std::move(t_handler);

    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        std::cerr << "Failed to get submission queue entry\n";
        m_pending_operations.erase(operation_id);
        return 0ul;
    }

    io_uring_prep_accept(sqe, t_listen_fd, t_address, t_address_length, 0);
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(operation_id));
    io_uring_submit(&m_ring);

    return operation_id;
}

auto IoUringContext::async_read(int t_fd, void* t_buffer, size_t t_length,
                    std::function<void(int)> t_handler) -> unsigned long
{
    const auto operation_id = m_next_operation_id++;
    m_pending_operations[operation_id] = std::move(t_handler);

    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        m_pending_operations.erase(operation_id);
        return 0ul;
    }

    io_uring_prep_read(sqe, t_fd, t_buffer, t_length, 0);
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(operation_id));
    io_uring_submit(&m_ring);

    return operation_id;
}

auto IoUringContext::async_write(int t_fd, const void* t_buffer, size_t t_length,
                    std::function<void(int)> t_handler) -> unsigned long
{
    const auto operation_id = m_next_operation_id++;
    m_pending_operations[operation_id] = std::move(t_handler);

    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        m_pending_operations.erase(operation_id);
        return 0ul;
    }

    io_uring_prep_write(sqe, t_fd, t_buffer, t_length, 0);
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(operation_id));
    io_uring_submit(&m_ring);

    return operation_id;
}

auto IoUringContext::async_close(int t_fd, std::function<void(int)> t_handler) -> unsigned long
{
    const auto operation_id = m_next_operation_id++;

    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        m_pending_operations.erase(operation_id);
        return 0ul;
    }

    io_uring_prep_close(sqe, t_fd);
    io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(operation_id));
    io_uring_submit(&m_ring);

    return operation_id;
}
}
