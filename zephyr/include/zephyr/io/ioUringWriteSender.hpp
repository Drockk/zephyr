#pragma once

#include "zephyr/io/ioUringContext.hpp"
#include <memory>
#include <string>

namespace zephyr::io
{
class IoUringWriteSender
{
public:
    IoUringWriteSender(std::shared_ptr<IoUringContext> t_context, int t_fd, std::string t_d)
        : m_io_ctx(t_context), m_socket_fd(t_fd), m_data(std::move(t_d)) {}

    template<typename Receiver>
    struct Operation
    {
        std::shared_ptr<IoUringContext> io_ctx;
        int socket_fd;
        Receiver rcv;

        std::string data;

        void start() noexcept
        {
            io_ctx->async_write(
                socket_fd,
                data.data(),
                data.size(),
                [rcv = std::move(rcv)](int result) mutable
                {
                    if (result > 0) {
                        ex::set_value(std::move(rcv), static_cast<ssize_t>(result));
                    } else {
                        ex::set_error(std::move(rcv),
                                std::make_exception_ptr(
                                    std::runtime_error("Write failed")));
                    }
                }
            );
        }
    };

    template<typename Receiver>
    Operation<Receiver> connect(Receiver t_rcv)
    {
        return Operation<Receiver>{m_io_ctx, m_socket_fd, std::move(t_rcv), std::move(m_data)};
    }

private:
    std::shared_ptr<IoUringContext> m_io_ctx;
    int m_socket_fd;
    std::string m_data;
};
}
