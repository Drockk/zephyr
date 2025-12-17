#pragma once

#include <stdexec/execution.hpp>
#include <arpa/inet.h>

namespace zephyr::udp
{
    template<typename Scheduler, typename PipelineFactory>
    requires std::invocable<PipelineFactory>
class UdpServer {
    using PipelineType = std::invoke_result_t<PipelineFactory>;
    
    int socket_fd_ = -1;
    Scheduler scheduler_;
    PipelineType pipeline_;
    std::shared_ptr<zephyr::io::IoUringContext> io_ctx_;
    std::shared_ptr<zephyr::context::Context> context_;
    std::atomic<bool> is_running_{true};
    
public:
    UdpServer(Scheduler sched, PipelineFactory factory,
              std::shared_ptr<zephyr::io::IoUringContext> io)
        : scheduler_(sched)
        , pipeline_(factory())
        , io_ctx_(std::move(io))
        , context_(std::make_shared<zephyr::context::Context>()) {}
    
    ~UdpServer() { stop(); }
    
    bool bind(uint16_t port) {
        socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) return false;
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (::bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(socket_fd_);
            return false;
        }
        
        std::cout << "[UDP Server] Bound to port " << port << "\n";
        return true;
    }
    
    void run() { receive_loop(); }
    
    void stop() {
        is_running_.store(false);
        io_ctx_->cancel();
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
        }
    }
    
private:
    void receive_loop() {
        if (!is_running_.load()) return;
        
        auto work = stdexec::schedule(scheduler_)
            | stdexec::then([this]() -> std::optional<std::pair<zephyr::udp::UdpProtocol::InputType, sockaddr_in>> {
                if (!is_running_.load()) return std::nullopt;
                
                std::array<std::byte, 65536> buffer;
                sockaddr_in client_addr{};
                
                auto n = io_ctx_->recvfrom(socket_fd_, buffer, client_addr);
                if (!is_running_.load() || n <= 0) return std::nullopt;
                
                // Get local port
                sockaddr_in local_addr{};
                socklen_t len = sizeof(local_addr);
                getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&local_addr), &len);
                
                zephyr::udp::UdpProtocol::InputType packet{
                    .data = {reinterpret_cast<uint8_t*>(buffer.data()),
                             reinterpret_cast<uint8_t*>(buffer.data()) + n},
                    .source_ip = inet_ntoa(client_addr.sin_addr),
                    .source_port = ntohs(client_addr.sin_port),
                    .dest_port = ntohs(local_addr.sin_port),
                    .client_addr = client_addr
                };
                
                std::cout << "[UDP Server] Received " << n << " bytes from "
                          << packet.source_ip << ":" << packet.source_port << "\n";
                
                return std::make_pair(std::move(packet), client_addr);
            })
            | stdexec::let_value([this](auto maybe_packet) {
                using Result = std::optional<std::pair<std::vector<uint8_t>, sockaddr_in>>;
                using Sender = zephyr::common::ResultSender<Result>;
                
                if (!maybe_packet) return Sender{stdexec::just(Result{})};
                
                auto [packet, addr] = std::move(*maybe_packet);
                return Sender{
                    pipeline_(std::move(packet), context_)
                        | stdexec::then([addr](zephyr::udp::UdpProtocol::OutputType response) -> Result {
                            if (response && !response->empty()) {
                                return std::make_pair(std::move(*response), addr);
                            }
                            return std::nullopt;
                        })
                };
            })
            | stdexec::then([this](auto maybe_response) {
                if (maybe_response) {
                    auto& [data, addr] = *maybe_response;
                    auto sent = io_ctx_->sendto(socket_fd_,
                        std::span<const std::byte>{reinterpret_cast<const std::byte*>(data.data()), data.size()},
                        addr);
                    std::cout << "[UDP Server] Sent " << sent << " bytes\n";
                }
                if (is_running_.load()) receive_loop();
            })
            | stdexec::upon_error([this](std::exception_ptr e) {
                try { std::rethrow_exception(e); }
                catch (const std::exception& ex) {
                    std::cout << "[UDP Server] Error: " << ex.what() << "\n";
                }
                if (is_running_.load()) receive_loop();
            });
        
        stdexec::start_detached(std::move(work));
    }
};

template<typename Scheduler, typename PipelineFactory>
auto make_udp_server(Scheduler sched, PipelineFactory&& factory,
                     std::shared_ptr<zephyr::io::IoUringContext> io) {
    return UdpServer<Scheduler, std::decay_t<PipelineFactory>>(
        sched, std::forward<PipelineFactory>(factory), std::move(io));
}
}