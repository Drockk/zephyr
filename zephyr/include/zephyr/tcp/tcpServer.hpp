#pragma once

#include <stdexec/execution.hpp>

namespace zephyr::tcp
{
template<typename Scheduler, typename PipelineFactory>
    requires std::invocable<PipelineFactory>
class TcpServer {
    using PipelineType = std::invoke_result_t<PipelineFactory>;
    using Session = zephyr::tcp::TcpSession<PipelineType>;
    
    int listen_socket_ = -1;
    Scheduler scheduler_;
    PipelineFactory pipeline_factory_;
    std::shared_ptr<zephyr::io::IoUringContext> io_ctx_;
    std::atomic<bool> is_running_{true};
    std::map<int, std::shared_ptr<Session>> sessions_;
    
public:
    TcpServer(Scheduler sched, PipelineFactory factory, 
              std::shared_ptr<zephyr::io::IoUringContext> io)
        : scheduler_(sched)
        , pipeline_factory_(std::move(factory))
        , io_ctx_(std::move(io)) {}
    
    ~TcpServer() { stop(); }
    
    bool listen(uint16_t port) {
        listen_socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket_ < 0) return false;
        
        int opt = 1;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        fcntl(listen_socket_, F_SETFL, O_NONBLOCK);
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (::bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
            ::listen(listen_socket_, 128) < 0) {
            ::close(listen_socket_);
            return false;
        }
        
        std::cout << "[TCP Server] Listening on port " << port << "\n";
        return true;
    }
    
    void run() { accept_loop(); }
    
    void stop() {
        is_running_.store(false);
        io_ctx_->cancel();
        if (listen_socket_ >= 0) {
            ::close(listen_socket_);
            listen_socket_ = -1;
        }
    }
    
private:
    void remove_session(int fd) {
        auto work = stdexec::schedule(scheduler_)
            | stdexec::then([this, fd] {
                if (auto it = sessions_.find(fd); it != sessions_.end()) {
                    std::cout << "[TCP Server] Removing session fd=" << fd << "\n";
                    sessions_.erase(it);
                }
            });
        stdexec::start_detached(std::move(work));
    }
    
    void accept_loop() {
        if (!is_running_.load()) return;
        
        auto work = stdexec::schedule(scheduler_)
            | stdexec::then([this] {
                if (!is_running_.load()) return;
                
                int client_fd = io_ctx_->accept(listen_socket_);
                if (!is_running_.load()) return;
                
                if (client_fd >= 0) {
                    std::cout << "[TCP Server] New connection: fd=" << client_fd << "\n";
                    auto session = std::make_shared<Session>(
                        client_fd, pipeline_factory_(), io_ctx_,
                        [this](int fd) { remove_session(fd); }
                    );
                    sessions_[client_fd] = session;
                    session->start();
                }
                
                if (is_running_.load()) accept_loop();
            })
            | stdexec::upon_error([this](std::exception_ptr) {
                if (is_running_.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    accept_loop();
                }
            });
        
        stdexec::start_detached(std::move(work));
    }
};

template<typename Scheduler, typename PipelineFactory>
auto make_tcp_server(Scheduler sched, PipelineFactory&& factory,
                     std::shared_ptr<zephyr::io::IoUringContext> io) {
    return TcpServer<Scheduler, std::decay_t<PipelineFactory>>(
        sched, std::forward<PipelineFactory>(factory), std::move(io));
}
}
