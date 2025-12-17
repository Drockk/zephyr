#pragma once

#include "zephyr/execution/strandScheduler.hpp"
#include "zephyr/pipeline/pipelineConcept.hpp"
#include "zephyr/tcp/tcpProtocol.hpp"
#include <exec/single_thread_context.hpp>
#include <zephyr/io/ioUringContext.hpp>
#include <iostream>

namespace zephyr::tcp
{
template<pipeline::PipelineConcept<TcpProtocol> Pipeline>
class TcpSession : public std::enable_shared_from_this<TcpSession<Pipeline>> {
public:
    using OnCloseCallback = std::function<void(int)>;
    
private:
    int socket_fd_;
    exec::single_thread_context strand_ctx_;
    execution::StrandScheduler<decltype(strand_ctx_.get_scheduler())> strand_;
    Pipeline pipeline_;
    std::shared_ptr<io::IoUringContext> io_ctx_;
    std::shared_ptr<context::Context> context_;
    OnCloseCallback on_close_;
    bool is_active_ = true;
    
public:
    TcpSession(int fd, Pipeline pipeline, std::shared_ptr<io::IoUringContext> io,
               OnCloseCallback on_close = nullptr)
        : socket_fd_(fd)
        , strand_(strand_ctx_.get_scheduler())
        , pipeline_(std::move(pipeline))
        , io_ctx_(std::move(io))
        , context_(std::make_shared<context::Context>())
        , on_close_(std::move(on_close)) 
    {
        std::cout << "[TCP:" << socket_fd_ << "] Session created\n";
    }
    
    ~TcpSession() {
        std::cout << "[TCP:" << socket_fd_ << "] Session destroyed\n";
        if (socket_fd_ >= 0) ::close(socket_fd_);
    }
    
    void start() { read_loop(); }
    void stop() { is_active_ = false; }
    int fd() const { return socket_fd_; }
    
private:
    void read_loop() {
        auto self = this->shared_from_this();
        
        auto work = stdexec::schedule(strand_)
            | stdexec::then([self]() -> std::optional<std::string> {
                std::array<char, 4096> buffer;
                auto n = self->io_ctx_->receive(self->socket_fd_, 
                    std::as_writable_bytes(std::span{buffer}));
                
                if (n > 0) return std::string(buffer.data(), n);
                if (n == 0) return std::nullopt;
                throw std::runtime_error("Read error");
            })
            | stdexec::let_value([self](std::optional<std::string> data) {
                if (!data) {
                    std::cout << "[TCP:" << self->socket_fd_ << "] Connection closed\n";
                    self->is_active_ = false;
                    if (self->on_close_) self->on_close_(self->socket_fd_);
                    return TcpProtocol::ResultSenderType{stdexec::just(TcpProtocol::OutputType{})};
                }
                
                std::cout << "[TCP:" << self->socket_fd_ << "] Received " << data->size() << " bytes\n";
                return self->pipeline_(std::move(*data), self->context_);
            })
            | stdexec::then([self](TcpProtocol::OutputType result) {
                if (result && !result->empty()) {
                    auto sent = self->io_ctx_->send(self->socket_fd_,
                        std::as_bytes(std::span{*result}));
                    std::cout << "[TCP:" << self->socket_fd_ << "] Sent " << sent << " bytes\n";
                }
                if (self->is_active_) self->read_loop();
            })
            | stdexec::upon_error([self](std::exception_ptr e) {
                try { std::rethrow_exception(e); }
                catch (const std::exception& ex) {
                    std::cout << "[TCP:" << self->socket_fd_ << "] Error: " << ex.what() << "\n";
                }
                self->is_active_ = false;
                if (self->on_close_) self->on_close_(self->socket_fd_);
            });
        
        stdexec::start_detached(std::move(work));
    }
};
}