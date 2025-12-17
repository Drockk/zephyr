#include "zephyr/io/ioUringContext.hpp"

#include <cerrno>
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
    cancel();
    io_uring_queue_exit(&m_ring);
}

auto IoUringContext::cancel() -> void
{
    m_cancelled.store(true);
    
    // Submit a NOP to wake up any waiting threads
    auto* sqe = io_uring_get_sqe(&m_ring);
    if (sqe) {
        io_uring_prep_nop(sqe);
        io_uring_submit(&m_ring);
    }
}

auto IoUringContext::accept(int32_t t_listen_fd) -> int
{
    if (m_cancelled.load()) return -1;
    
    sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        return -1;
    }

    io_uring_prep_accept(sqe, t_listen_fd, reinterpret_cast<sockaddr*>(&addr), &addrlen, SOCK_NONBLOCK);
    io_uring_submit(&m_ring);

    io_uring_cqe* cqe{ nullptr };
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    auto result = cqe->res;
    io_uring_cqe_seen(&m_ring, cqe);

    if (m_cancelled.load()) return -1;
    
    return result;
}

auto IoUringContext::receive(int32_t t_fd, std::span<std::byte> t_buffer) -> ssize_t
{
    if (m_cancelled.load()) return -1;
    
    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe){
        return -1;
    }

    io_uring_prep_recv(sqe, t_fd, t_buffer.data(), t_buffer.size(), 0);
    io_uring_submit(&m_ring);

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    ssize_t res = cqe->res;
    io_uring_cqe_seen(&m_ring, cqe);

    if (m_cancelled.load()) return -1;

    return res;
}

auto IoUringContext::send(int32_t t_fd, const std::span<const std::byte> t_buffer) -> ssize_t
{
    if (m_cancelled.load()) return -1;
    
    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        return -1;
    }

    io_uring_prep_send(sqe, t_fd, t_buffer.data(), t_buffer.size(), 0);
    io_uring_submit(&m_ring);

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    ssize_t res = cqe->res;
    io_uring_cqe_seen(&m_ring, cqe);

    return res;
}

auto IoUringContext::recvfrom(int32_t t_fd, std::span<std::byte> t_buffer, sockaddr_in& t_addr) -> ssize_t
{
    if (m_cancelled.load()) return -1;
    
    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        return -1;
    }

    msghdr msg{};
    iovec iov{};
    iov.iov_base = t_buffer.data();
    iov.iov_len = t_buffer.size();
    
    msg.msg_name = &t_addr;
    msg.msg_namelen = sizeof(t_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    io_uring_prep_recvmsg(sqe, t_fd, &msg, 0);
    io_uring_submit(&m_ring);

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    ssize_t res = cqe->res;
    io_uring_cqe_seen(&m_ring, cqe);

    if (m_cancelled.load()) return -1;

    return res;
}

auto IoUringContext::sendto(int32_t t_fd, const std::span<const std::byte> t_buffer, const sockaddr_in& t_addr) -> ssize_t
{
    if (m_cancelled.load()) return -1;
    
    auto* sqe = io_uring_get_sqe(&m_ring);
    if (!sqe) {
        return -1;
    }

    msghdr msg{};
    iovec iov{};
    iov.iov_base = const_cast<std::byte*>(t_buffer.data());
    iov.iov_len = t_buffer.size();
    
    msg.msg_name = const_cast<sockaddr_in*>(&t_addr);
    msg.msg_namelen = sizeof(t_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    io_uring_prep_sendmsg(sqe, t_fd, &msg, 0);
    io_uring_submit(&m_ring);

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&m_ring, &cqe) != 0) {
        return -1;
    }

    ssize_t res = cqe->res;
    io_uring_cqe_seen(&m_ring, cqe);

    return res;
}
}
