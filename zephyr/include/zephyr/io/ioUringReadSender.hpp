#pragma once

#include "zephyr/io/ioUringContext.hpp"
#include <memory>
#include <stdexec/execution.hpp>

namespace zephyr::io
{
class IoUringReadSender
{
public:
    IoUringReadSender(std::shared_ptr<IoUringContext> t_context, int t_fd, size_t t_max)
                        : m_io_ctx(t_context), m_socket_fd(t_fd), m_max_bytes(t_max) {}

    template<typename Receiver>
    struct Operation
    {
        std::shared_ptr<IoUringContext> io_ctx;
        int socket_fd;
        Receiver rcv;

        std::unique_ptr<char[]> buffer;
        size_t buffer_size;
    
        Operation(std::shared_ptr<IoUringContext> ctx, int fd, size_t size, Receiver r)
            : io_ctx(ctx), socket_fd(fd), rcv(std::move(r)), buffer(new char[size]), buffer_size(size) {}
    
        void start() noexcept
        {
            io_ctx->async_read(
                socket_fd,
                buffer.get(),
                buffer_size,

                [rcv = std::move(rcv), buf = std::move(buffer), size = buffer_size] (int result) mutable
                {
                    if (result > 0) {
                        std::string data(buf.get(), result);
                        stdexec::set_value(std::move(rcv), std::move(data));
                    } else if (result == 0) {
                        stdexec::set_done(std::move(rcv));
                    } else {
                        stdexec::set_error(std::move(rcv),
                            std::make_exception_ptr(
                                std::runtime_error("Read failed")));
                    }
                }
            );
        }
    };

    template<typename Receiver>
    Operation<Receiver> connect(Receiver t_rcv)
    {
        return Operation<Receiver>{m_io_ctx, m_socket_fd, m_max_bytes, std::move(t_rcv)};
    }

private:
    std::shared_ptr<IoUringContext> m_io_ctx;
    int m_socket_fd;
    size_t m_max_bytes;
};
}