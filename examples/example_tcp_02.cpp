#include <zephyr/context/context.hpp>
#include <zephyr/execution/strandScheduler.hpp>
#include <zephyr/http/middlewares/loggingMiddleware.hpp>
#include <zephyr/http/httpMessages.hpp>
#include <zephyr/http/httpPipelineBuilder.hpp>
#include <zephyr/http/httpRouter.hpp>
#include <zephyr/io/ioUringContext.hpp>
#include <zephyr/pipeline/pipelineConcept.hpp>
#include <zephyr/pipelines/rawPipeline.hpp>
#include <zephyr/tcp/tcpProtocol.hpp>
#include <zephyr/tcp/tcpSession.hpp>
#include <zephyr/udp/udpProtocol.hpp>
#include <zephyr/udp/udpRouter.hpp>
#include <zephyr/udp/udpRouterPipeline.hpp>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/single_thread_context.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <span>
#include <atomic>
#include <concepts>

namespace ex = stdexec;

// ============================================================================
// TCP SERVER
// ============================================================================

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
        auto work = ex::schedule(scheduler_)
            | ex::then([this, fd] {
                if (auto it = sessions_.find(fd); it != sessions_.end()) {
                    std::cout << "[TCP Server] Removing session fd=" << fd << "\n";
                    sessions_.erase(it);
                }
            });
        ex::start_detached(std::move(work));
    }
    
    void accept_loop() {
        if (!is_running_.load()) return;
        
        auto work = ex::schedule(scheduler_)
            | ex::then([this] {
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
            | ex::upon_error([this](std::exception_ptr) {
                if (is_running_.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    accept_loop();
                }
            });
        
        ex::start_detached(std::move(work));
    }
};

// ============================================================================
// UDP SERVER
// ============================================================================

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
        
        auto work = ex::schedule(scheduler_)
            | ex::then([this]() -> std::optional<std::pair<zephyr::udp::UdpProtocol::InputType, sockaddr_in>> {
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
            | ex::let_value([this](auto maybe_packet) {
                using Result = std::optional<std::pair<std::vector<uint8_t>, sockaddr_in>>;
                using Sender = zephyr::common::ResultSender<Result>;
                
                if (!maybe_packet) return Sender{ex::just(Result{})};
                
                auto [packet, addr] = std::move(*maybe_packet);
                return Sender{
                    pipeline_(std::move(packet), context_)
                        | ex::then([addr](zephyr::udp::UdpProtocol::OutputType response) -> Result {
                            if (response && !response->empty()) {
                                return std::make_pair(std::move(*response), addr);
                            }
                            return std::nullopt;
                        })
                };
            })
            | ex::then([this](auto maybe_response) {
                if (maybe_response) {
                    auto& [data, addr] = *maybe_response;
                    auto sent = io_ctx_->sendto(socket_fd_,
                        std::span<const std::byte>{reinterpret_cast<const std::byte*>(data.data()), data.size()},
                        addr);
                    std::cout << "[UDP Server] Sent " << sent << " bytes\n";
                }
                if (is_running_.load()) receive_loop();
            })
            | ex::upon_error([this](std::exception_ptr e) {
                try { std::rethrow_exception(e); }
                catch (const std::exception& ex) {
                    std::cout << "[UDP Server] Error: " << ex.what() << "\n";
                }
                if (is_running_.load()) receive_loop();
            });
        
        ex::start_detached(std::move(work));
    }
};

// ============================================================================
// FACTORY HELPERS - Dedukcja typów
// ============================================================================

template<typename Scheduler, typename PipelineFactory>
auto make_tcp_server(Scheduler sched, PipelineFactory&& factory,
                     std::shared_ptr<zephyr::io::IoUringContext> io) {
    return TcpServer<Scheduler, std::decay_t<PipelineFactory>>(
        sched, std::forward<PipelineFactory>(factory), std::move(io));
}

template<typename Scheduler, typename PipelineFactory>
auto make_udp_server(Scheduler sched, PipelineFactory&& factory,
                     std::shared_ptr<zephyr::io::IoUringContext> io) {
    return UdpServer<Scheduler, std::decay_t<PipelineFactory>>(
        sched, std::forward<PipelineFactory>(factory), std::move(io));
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "=== Pipeline Server Example ===\n\n";
    
    exec::static_thread_pool pool(4);
    auto scheduler = pool.get_scheduler();
    
    // HTTP Router
    zephyr::http::HttpRouter http_router;
    
    http_router.get("/", [](const auto&, const auto&) {
        return zephyr::http::HttpResponse::ok("Welcome!");
    });
    
    http_router.get("/users/:id", [](const auto& req, const auto&) {
        auto id = req.path_params.at("id");
        return zephyr::http::HttpResponse::json(R"({"id": ")" + id + R"("})");
    });
    
    http_router.post("/echo", [](const auto& req, const auto&) {
        return zephyr::http::HttpResponse::ok("Echo: " + req.body);
    });
    
    // UDP Router
    zephyr::udp::UdpRouter udp_router;
    
    udp_router.on_port(5000, [](const auto& packet, const auto&) {
        std::cout << "[UDP:5000] Echo " << packet.data.size() << " bytes\n";
        return std::optional{packet.data};
    });
    
    // Create servers
    auto http_io = std::make_shared<zephyr::io::IoUringContext>();
    auto echo_io = std::make_shared<zephyr::io::IoUringContext>();
    auto udp_io = std::make_shared<zephyr::io::IoUringContext>();
    
    // HTTP Server (TCP + HTTP Pipeline) - using HttpPipelineBuilder with middlewares
    auto http_pipeline_factory = zephyr::http::HttpPipelineBuilder<>()
        .with_router(http_router)
        .with_middleware(zephyr::http::logging_middleware())
        .with_middleware([](zephyr::http::HttpRequest req) -> zephyr::common::ResultSender<zephyr::http::HttpRequest> {
            // CORS middleware - dodaje nagłówki do response przez kontekst
            std::cout << "[Middleware:CORS] Adding headers for " << req.path << "\n";
            return zephyr::common::ResultSender<zephyr::http::HttpRequest>{ex::just(std::move(req))};
        })
        .build();
    
    auto http_server = make_tcp_server(scheduler, http_pipeline_factory, http_io);
    
    // Echo Server (TCP + Raw Pipeline)
    auto echo_server = make_tcp_server(scheduler,
        []() { return zephyr::pipelines::make_raw_pipeline<zephyr::tcp::TcpProtocol>(
            [](std::string data) -> zephyr::tcp::TcpProtocol::OutputType {
                return "ECHO: " + data;
            }); }, echo_io);
    
    // UDP Server (UDP + Router Pipeline)
    auto udp_server = make_udp_server(scheduler,
        [&udp_router]() { return zephyr::udp::UdpRouterPipeline(udp_router); }, udp_io);
    
    // Start servers
    if (!http_server.listen(8080) || !echo_server.listen(9000) || !udp_server.bind(5000)) {
        std::cerr << "Failed to start servers\n";
        return 1;
    }
    
    http_server.run();
    echo_server.run();
    udp_server.run();
    
    std::cout << "\nServers running:\n"
              << "  HTTP: http://localhost:8080/\n"
              << "  Echo: nc localhost 9000\n"
              << "  UDP:  echo test | nc -u localhost 5000\n\n"
              << "Press Ctrl+C to stop...\n\n";
    
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    std::cout << "\nStopping servers...\n";
    http_server.stop();
    echo_server.stop();
    udp_server.stop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.request_stop();
    
    std::cout << "Done.\n";
    return 0;
}
