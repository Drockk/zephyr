#include "zephyr/io/ioUringContext.hpp"

#include <netinet/in.h>
#include <stdexcept>

namespace zephyr::io
{
IoUringContext::IoUringContext(uint32_t t_entries)
{
    if (io_uring_queue_init(t_entries, &m_ring, 0) != 0) {
        throw std::runtime_error("io_uring_queue_init failed");
    }
}

IoUringContext::~IoUringContext()
{
    io_uring_queue_exit(&m_ring);
}

auto IoUringContext::accept(int32_t t_listen_fd) -> int
{
    sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        return -1;
    }

    io_uring_prep_accept(sqe, t_listen_fd, reinterpret_cast<sockaddr*>(&addr), &addrlen, SOCK_NONBLOCK);
    io_uring_submit_and_wait(&m_ring, 1);

    io_uring_cqe* cqe{ nullptr };
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    auto respond = cqe->res;

    io_uring_cqe_seen(&m_ring, cqe);

    return respond;
}

auto IoUringContext::receive(int32_t t_fd, std::span<std::byte> t_buffer) -> ssize_t
{
    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe){
        return -1;
    }

    io_uring_prep_recv(sqe, t_fd, t_buffer.data(), t_buffer.size(), 0);
    io_uring_submit_and_wait(&m_ring, 1);

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    ssize_t res = cqe->res;

    io_uring_cqe_seen(&m_ring, cqe);

    return res;
}

auto IoUringContext::send(int32_t t_fd, const std::span<const std::byte> t_buffer) -> ssize_t
{
    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        return -1;
    }

    io_uring_prep_send(sqe, t_fd, t_buffer.data(), t_buffer.size(), 0);
    io_uring_submit_and_wait(&m_ring, 1);

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    ssize_t res = cqe->res;

    io_uring_cqe_seen(&m_ring, cqe);

    return res;
}
}
