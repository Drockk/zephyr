#pragma once

#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdexcept>
#include <stdexec/execution.hpp>

#include "ioUringContext.hpp"

namespace zephyr::io
{
namespace exec = stdexec;

class IoUringAcceptSender
{
public:
    IoUringAcceptSender(std::shared_ptr<IoUringContext> ctx, int fd)
        : io_ctx(ctx), listen_fd(fd) {}

    template<typename Receiver>
    struct Operation
    {
        std::shared_ptr<IoUringContext> io_ctx;
        int listen_fd;
        Receiver rcv;

        sockaddr_in client_addr;
        socklen_t addr_len;
    
        void start() noexcept
        {
            addr_len = sizeof(client_addr);
        
            io_ctx->async_accept(
                listen_fd,
                (sockaddr*)&client_addr,
                &addr_len,
                [rcv = std::move(rcv)](int result) mutable {
                    if (result >= 0) {
                        exec::set_value(std::move(rcv), result);
                    } else {
                        exec::set_error(std::move(rcv),
                                    std::make_exception_ptr(
                                        std::runtime_error("Accept failed")));
                    }
                }
            );
        }
    };

    template<typename Receiver>
    Operation<Receiver> connect(Receiver rcv)
    {
        return Operation<Receiver>{io_ctx, listen_fd, std::move(rcv)};
    }

private:
    std::shared_ptr<IoUringContext> io_ctx;
    int listen_fd;
};

} // namespace zephyr::io
