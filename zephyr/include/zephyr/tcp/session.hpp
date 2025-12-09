#pragma once

#include "zephyr/execution/strandScheduler.hpp"
#include "zephyr/io/ioUringContext.hpp"
#include <functional>
#include <memory>

namespace zephyr::tcp
{
class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(int sock, execution::StrandScheduler s, std::shared_ptr<io::IoUringContext> ctx,
            std::function<std::string(const std::string&)> handler)
                : client_socket(sock), strand(s), io_ctx(ctx),
    data_handler(std::move(handler)) {}

    ~Session()
    {
        if (client_socket >= 0 && io_ctx) {
            io_ctx->async_close(client_socket, [](int result) {
                if (result == 0) {
                }
            });
        }
    }

    void start()
    {
        auto self = shared_from_this();
    
        auto work = stdexec::schedule(strand)
            | stdexec::let_value([self]() {
                return IoUringReadSender(self->io_ctx, self->client_socket, 4096);
            })
            | stdexec::then([self](std::string data) {
                if (data.empty()) {
                    return std::string{};
                }

                self->receive_buffer += data;
                self->requests_handled++;

                try {
                    std::string response = self->data_handler(self->receive_buffer);

                    if (!response.empty()) {
                        self->receive_buffer.clear();
                        return response;
                    }
                } catch (const std::exception& e) {
                    self->is_active = false;
                }
            
                return std::string{};
            })
            | stdexec::let_value([self](std::string response) {
                if (!response.empty()) {
                    return IoUringWriteSender(self->io_ctx, self->client_socket, response);
                }
                return stdexec::just(0L);
            })
            | ex::then([self](ssize_t bytes_written) {
                if (bytes_written > 0) {
                }

                if (self->is_active) {
                    self->start();
                }
            })
            | stdexec::upon_stopped([self]() {
                self->is_active = false;
            })
            | stdexec::upon_error([self](std::exception_ptr eptr) {
                try {
                    std::rethrow_exception(eptr);
                } catch (const std::exception& e) {
                }
                self->is_active = false;
            });

        stdexec::start_detached(std::move(work));
    }

    void stop()
    {
        is_active = false;
    }

private:
    int client_socket;
    StrandScheduler strand;
    std::shared_ptr<IoUringContext> io_ctx;

    std::function<std::string(const std::string&)> data_handler;

    std::string receive_buffer;
    int requests_handled = 0;
    bool is_active = true;
};
}
