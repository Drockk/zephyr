#include <zephyr/context/context.hpp>
#include <zephyr/execution/strandOp.hpp>
#include <zephyr/execution/strandState.hpp>
#include <zephyr/http/httpMessages.hpp>
#include <zephyr/http/httpParser.hpp>
#include <zephyr/io/ioUringContext.hpp>
#include <zephyr/udp/udpPacket.hpp>
#include <zephyr/utils/anySender.hpp>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/inline_scheduler.hpp>
#include <exec/single_thread_context.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <regex>
#include <optional>
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <span>

namespace ex = stdexec;

// ============================================================================
// HTTP PARSER I SERIALIZER
// ============================================================================

class HttpSerializer {
public:
    static std::string serialize(const zephyr::http::HttpResponse& resp) {
        std::string result = "HTTP/1.1 ";
        result += std::to_string(resp.status_code) + " ";
        result += resp.status_text + "\r\n";
        
        auto headers = resp.headers;
        if (headers.find("Content-Length") == headers.end() && !resp.body.empty()) {
            headers["Content-Length"] = std::to_string(resp.body.size());
        }
        
        for (const auto& [name, value] : headers) {
            result += name + ": " + value + "\r\n";
        }
        
        result += "\r\n" + resp.body;
        return result;
    }
};

// ============================================================================
// HTTP ROUTER - Pattern matching i routing
// ============================================================================

class HttpRoute {
public:
    using HttpSender = zephyr::utils::any_sender_of<
        ex::set_value_t(zephyr::http::HttpResponse),
        ex::set_error_t(std::exception_ptr),
        ex::set_stopped_t()
    >;
    using Handler = std::function<HttpSender(const zephyr::http::HttpRequest&, const zephyr::context::Context&)>;
    
private:
    std::string method;
    std::regex path_regex;
    std::vector<std::string> param_names;
    Handler handler;
    
public:
    template<typename H>
    HttpRoute(std::string m, std::string pattern, H h) 
        : method(std::move(m)), handler(std::move(h)) {
        compile_pattern(pattern);
    }
    
    bool matches(const std::string& req_method, const std::string& req_path) const {
        if (method != "*" && method != req_method) return false;
        return std::regex_match(req_path, path_regex);
    }
    
    std::map<std::string, std::string> extract_params(const std::string& path) const {
        std::map<std::string, std::string> params;
        std::smatch match;
        if (std::regex_match(path, match, path_regex)) {
            for (size_t i = 0; i < param_names.size() && i + 1 < match.size(); ++i) {
                params[param_names[i]] = match[i + 1];
            }
        }
        return params;
    }
    
    HttpSender invoke(zephyr::http::HttpRequest req, const zephyr::context::Context& ctx) const {
        req.path_params = extract_params(req.path);
        return handler(req, ctx);
    }
    
private:
    void compile_pattern(const std::string& pattern) {
        std::string regex_pattern;
        size_t pos = 0;
        
        while (pos < pattern.size()) {
            if (pattern[pos] == ':') {
                size_t end = pattern.find('/', pos);
                if (end == std::string::npos) end = pattern.size();
                
                std::string param_name = pattern.substr(pos + 1, end - pos - 1);
                param_names.push_back(param_name);
                regex_pattern += "([^/]+)";
                pos = end;
            } else if (pattern[pos] == '*') {
                regex_pattern += ".*";
                pos++;
            } else {
                if (std::string(".+?^$()[]{}|\\").find(pattern[pos]) != std::string::npos) {
                    regex_pattern += '\\';
                }
                regex_pattern += pattern[pos];
                pos++;
            }
        }
        path_regex = std::regex(regex_pattern);
    }
};

class HttpRouter {
    using HttpSender = HttpRoute::HttpSender;
    std::vector<HttpRoute> routes;
    std::shared_ptr<zephyr::context::Context> context;
    
public:
    HttpRouter() : context(std::make_shared<zephyr::context::Context>()) {}
    
    template<typename T>
    void add_resource(const std::string& name, std::shared_ptr<T> res) {
        context->set(name, res);
    }
    
    // Synchroniczne handlery
    template<typename SyncHandler>
    void add_route(std::string method, std::string path, SyncHandler handler) {
        auto async_handler = [h = std::move(handler)]
            (const zephyr::http::HttpRequest& req, const zephyr::context::Context& ctx) {
            return HttpSender{ex::just(h(req, ctx))};
        };
        routes.emplace_back(std::move(method), std::move(path), std::move(async_handler));
    }
    
    void get(std::string path, auto handler) {
        add_route("GET", std::move(path), std::move(handler));
    }
    
    void post(std::string path, auto handler) {
        add_route("POST", std::move(path), std::move(handler));
    }
    
    // Asynchroniczne handlery
    template<typename AsyncHandler>
    void get_async(std::string path, AsyncHandler handler) {
        routes.emplace_back(
            "GET",
            std::move(path),
            [h = std::move(handler)](const zephyr::http::HttpRequest& req, const zephyr::context::Context& ctx) {
                return HttpSender{h(req, ctx)};
            }
        );
    }
    
    HttpSender route(zephyr::http::HttpRequest req) const {
        for (const auto& r : routes) {
            if (r.matches(req.method, req.path)) {
                return r.invoke(std::move(req), *context);
            }
        }
        return HttpSender{ex::just(zephyr::http::HttpResponse::not_found())};
    }
};

// ============================================================================
// UDP ROUTER - Routing po portach
// ============================================================================

class UdpRouter {
public:
    using UdpSender = zephyr::utils::any_sender_of<
        ex::set_value_t(std::optional<std::vector<uint8_t>>),
        ex::set_error_t(std::exception_ptr),
        ex::set_stopped_t()
    >;
    using Handler = std::function<UdpSender(const zephyr::udp::UdpPacket&, const zephyr::context::Context&)>;
    
private:
    struct Route {
        int port;
        Handler handler;
    };
    
    std::vector<Route> routes;
    std::shared_ptr<zephyr::context::Context> context;
    
public:
    UdpRouter() : context(std::make_shared<zephyr::context::Context>()) {}
    
    template<typename SyncHandler>
    void on_port(int port, SyncHandler handler) {
        auto async_handler = [h = std::move(handler)]
            (const zephyr::udp::UdpPacket& packet, const zephyr::context::Context& ctx) {
            return UdpSender{ex::just(h(packet, ctx))};
        };
        routes.push_back(Route{port, std::move(async_handler)});
    }
    
    UdpSender route(zephyr::udp::UdpPacket packet) const {
        for (const auto& r : routes) {
            if (r.port == packet.dest_port) {
                return r.handler(packet, *context);
            }
        }
        return UdpSender{ex::just(std::optional<std::vector<uint8_t>>{})};
    }
};

// ============================================================================
// STRAND - prosta serializacja zadań na bazowym schedulerze
// ============================================================================

template<typename BaseScheduler>
struct StrandSender {
    std::shared_ptr<zephyr::execution::StrandState<BaseScheduler>> st;
    using completion_signatures = ex::completion_signatures<
        ex::set_value_t(),
        ex::set_error_t(std::exception_ptr),
        ex::set_stopped_t()
    >;

    template<class Receiver>
    zephyr::execution::StrandOp<BaseScheduler, Receiver> connect(Receiver r) const {
        return zephyr::execution::StrandOp<BaseScheduler, Receiver>{st, std::move(r)};
    }
};

template<typename BaseScheduler>
class StrandScheduler {
public:
    explicit StrandScheduler(BaseScheduler base)
        : state_(std::make_shared<zephyr::execution::StrandState<BaseScheduler>>(std::move(base))) {}

    friend auto tag_invoke(ex::schedule_t, const StrandScheduler& sched) {
        return StrandSender<BaseScheduler>{sched.state_};
    }

    friend bool operator==(const StrandScheduler& a, const StrandScheduler& b) {
        return a.state_.get() == b.state_.get();
    }
    friend bool operator!=(const StrandScheduler& a, const StrandScheduler& b) {
        return !(a == b);
    }

private:
    std::shared_ptr<zephyr::execution::StrandState<BaseScheduler>> state_;
};

template<class Base>
inline constexpr bool stdexec::enable_sender<StrandSender<Base>> = true;

// ============================================================================
// TCP SESSION - Pojedyncze połączenie klienta
// ============================================================================

class TcpSession : public std::enable_shared_from_this<TcpSession> {
    int socket_fd;
    exec::single_thread_context strand_ctx;
    using SessionScheduler = StrandScheduler<decltype(strand_ctx.get_scheduler())>;
    SessionScheduler strand;
    const HttpRouter& router;
    std::shared_ptr<zephyr::io::IoUringContext> io_ctx;
    
    std::string receive_buffer;
    bool is_active = true;
    
public:
    TcpSession(int fd, const HttpRouter& r, std::shared_ptr<zephyr::io::IoUringContext> io)
        : socket_fd(fd), strand_ctx(), strand(SessionScheduler{strand_ctx.get_scheduler()}), router(r), io_ctx(std::move(io)) {
        std::cout << "[TCP Session " << socket_fd << "] Created\n";
    }
    
    ~TcpSession() {
        std::cout << "[TCP Session " << socket_fd << "] Destroyed\n";
        if (socket_fd >= 0) {
            close(socket_fd);
        }
    }
    
    void start() {
        handle_read();
    }
    
    void stop() {
        is_active = false;
    }
    
private:
    void handle_read() {
        auto self = this->shared_from_this();

        auto work = ex::schedule(strand)
            | ex::then([self]() {
                char buffer[4096];
                ssize_t n = self->io_ctx->receive(self->socket_fd, std::span<std::byte>(reinterpret_cast<std::byte*>(buffer), sizeof(buffer)));
                if (n > 0) {
                    return std::string(buffer, n);
                } else if (n == 0) {
                    throw std::runtime_error("Connection closed");
                } else {
                    throw std::runtime_error("Read error");
                }
            })
            | ex::then([self](std::string data) -> std::optional<zephyr::http::HttpRequest> {
                if (data.empty()) return std::nullopt;
                
                self->receive_buffer += data;
                std::cout << "[TCP Session " << self->socket_fd << "] Received " 
                         << data.size() << " bytes\n";
                
                if (zephyr::http::HttpParser::is_complete(self->receive_buffer)) {
                    auto req = zephyr::http::HttpParser::parse(self->receive_buffer);
                    self->receive_buffer.clear();
                    return req;
                }
                return std::nullopt;
            })
            | ex::let_value([self](std::optional<zephyr::http::HttpRequest> maybe_req) {
                using ResponseSender = zephyr::utils::any_sender_of<
                    ex::set_value_t(std::string),
                    ex::set_error_t(std::exception_ptr),
                    ex::set_stopped_t()
                >;

                if (!maybe_req) {
                    // Niepełne żądanie - kontynuuj czytanie
                    return ResponseSender{ex::just(std::string{})};
                }
                
                std::cout << "[TCP Session " << self->socket_fd << "] Complete request: "
                         << maybe_req->method << " " << maybe_req->path << "\n";
                
                // Routuj przez HTTP router
                return ResponseSender{
                    self->router.route(*maybe_req)
                        | ex::then([](zephyr::http::HttpResponse resp) {
                            return HttpSerializer::serialize(resp);
                        })
                };
            })
            | ex::then([self](std::string response) {
                if (!response.empty()) {
                    ssize_t sent = self->io_ctx->send(self->socket_fd, std::span<std::byte>(reinterpret_cast<std::byte*>(response.data()), response.size()));
                    std::cout << "[TCP Session " << self->socket_fd << "] Sent " 
                             << sent << " bytes\n";
                }
                
                // Kontynuuj obsługę kolejnych żądań
                if (self->is_active) {
                    self->handle_read();
                }
            })
            | ex::upon_error([self](std::exception_ptr eptr) {
                try {
                    std::rethrow_exception(eptr);
                } catch (const std::exception& e) {
                    std::cout << "[TCP Session " << self->socket_fd << "] Error: " 
                             << e.what() << "\n";
                }
                self->is_active = false;
            });
        
        ex::start_detached(std::move(work));
    }
};

// ============================================================================
// TCP SERVER - Akceptuje połączenia
// ============================================================================

template<typename BaseScheduler>
class TcpServer {
    int listen_socket = -1;
    BaseScheduler pool_scheduler;
    const HttpRouter& router;
    bool is_running = true;
    std::shared_ptr<zephyr::io::IoUringContext> io_ctx;

    using Session = TcpSession;
    std::map<int, std::shared_ptr<Session>> sessions;
    
public:
    TcpServer(BaseScheduler sched, const HttpRouter& r, std::shared_ptr<zephyr::io::IoUringContext> io)
        : pool_scheduler(sched), router(r), io_ctx(std::move(io)) {}
    
    ~TcpServer() {
        stop();
    }
    
    bool listen(int port) {
        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket < 0) return false;
        
        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Nieblokujący socket dla accept
        fcntl(listen_socket, F_SETFL, O_NONBLOCK);
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(listen_socket, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(listen_socket);
            return false;
        }
        
        if (::listen(listen_socket, 128) < 0) {
            close(listen_socket);
            return false;
        }
        
        std::cout << "[TCP Server] Listening on port " << port << "\n";
        return true;
    }
    
    void run() {
        accept_loop();
    }
    
    void stop() {
        is_running = false;
        if (listen_socket >= 0) {
            close(listen_socket);
            listen_socket = -1;
        }
    }
    
private:
    void accept_loop() {
        auto work = ex::schedule(pool_scheduler)
            | ex::then([this]() {
                int client_fd = io_ctx->accept(listen_socket);
                
                if (client_fd >= 0) {
                    std::cout << "[TCP Server] Accepted connection: fd=" << client_fd << "\n";
                    auto session = std::make_shared<Session>(
                        client_fd, router, io_ctx
                    );
                    sessions[client_fd] = session;
                    session->start();
                } else {
                    std::cerr << "[TCP Server] Accept error: " << strerror(errno) << "\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                if (is_running) {
                    accept_loop();
                }
            })
            | ex::upon_error([this](std::exception_ptr) {
                if (is_running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    accept_loop();
                }
            });
        
        ex::start_detached(std::move(work));
    }
};

// ============================================================================
// UDP SERVER - Obsługa datagramów
// ============================================================================

template<typename BaseScheduler>
class UdpServer {
    int socket_fd = -1;
    BaseScheduler pool_scheduler;
    const UdpRouter& router;
    bool is_running = true;
    
public:
    UdpServer(BaseScheduler sched, const UdpRouter& r)
        : pool_scheduler(sched), router(r) {}
    
    ~UdpServer() {
        stop();
    }
    
    bool bind(int port) {
        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd < 0) return false;
        
        fcntl(socket_fd, F_SETFL, O_NONBLOCK);
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (::bind(socket_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(socket_fd);
            return false;
        }
        
        std::cout << "[UDP Server] Bound to port " << port << "\n";
        return true;
    }
    
    void run() {
        receive_loop();
    }
    
    void stop() {
        is_running = false;
        if (socket_fd >= 0) {
            close(socket_fd);
            socket_fd = -1;
        }
    }
    
private:
    void receive_loop() {
        // Każdy pakiet obsługiwany w swoim strandzie dla bezpieczeństwa
        auto work = ex::schedule(pool_scheduler)
            | ex::then([this]() -> std::optional<zephyr::udp::UdpPacket> {
                char buffer[65536];
                sockaddr_in client_addr{};
                socklen_t addr_len = sizeof(client_addr);
                
                ssize_t n = recvfrom(socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT,
                                    (sockaddr*)&client_addr, &addr_len);
                
                if (n > 0) {
                    zephyr::udp::UdpPacket packet;
                    packet.source_ip = inet_ntoa(client_addr.sin_addr);
                    packet.source_port = ntohs(client_addr.sin_port);
                    
                    sockaddr_in local_addr{};
                    addr_len = sizeof(local_addr);
                    getsockname(socket_fd, (sockaddr*)&local_addr, &addr_len);
                    packet.dest_port = ntohs(local_addr.sin_port);
                    
                    packet.data = std::vector<uint8_t>(buffer, buffer + n);
                    
                    std::cout << "[UDP Server] Received " << n << " bytes from "
                             << packet.source_ip << ":" << packet.source_port << "\n";
                    
                    return packet;
                }
                return std::nullopt;
            })
            | ex::let_value([this](std::optional<zephyr::udp::UdpPacket> maybe_packet) {
                using RouterSender = UdpRouter::UdpSender;

                if (!maybe_packet) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    return RouterSender{ex::just(std::optional<std::vector<uint8_t>>{})};
                }
                
                // Routuj przez UDP router
                return RouterSender{router.route(*maybe_packet)};
            })
            | ex::then([this](std::optional<std::vector<uint8_t>> maybe_response) {
                if (maybe_response && !maybe_response->empty()) {
                    // W produkcji: wysłalibyśmy odpowiedź z powrotem do klienta
                    std::cout << "[UDP Server] Would send " << maybe_response->size() 
                             << " bytes response\n";
                }
                
                if (is_running) {
                    receive_loop();
                }
            })
            | ex::upon_error([this](std::exception_ptr) {
                if (is_running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    receive_loop();
                }
            });
        
        ex::start_detached(std::move(work));
    }
};

// ============================================================================
// PRZYKŁAD UŻYCIA
// ============================================================================

int main() {
    std::cout << "=== TCP/UDP/HTTP Server z exec ===\n\n";
    
    // Tworzymy pulę 4 wątków roboczych
    exec::static_thread_pool pool(4);
    auto pool_scheduler = pool.get_scheduler();
    
    // ========================================================================
    // Konfiguracja HTTP Router
    // ========================================================================
    
    HttpRouter http_router;
    
    // Prosty endpoint
    http_router.get("/", [](const zephyr::http::HttpRequest&, const zephyr::context::Context&) {
        return zephyr::http::HttpResponse::ok("Welcome to the server!");
    });
    
    // Endpoint z parametrami
    http_router.get("/users/:id", [](const zephyr::http::HttpRequest& req, const zephyr::context::Context&) {
        std::string user_id = req.path_params.at("id");
        std::string json = R"({"id": ")" + user_id + R"(", "name": "User )" + user_id + R"("})";
        return zephyr::http::HttpResponse::json(json);
    });
    
    // Asynchroniczny endpoint - symuluje zapytanie do bazy
    http_router.get_async("/async", [pool_scheduler](const zephyr::http::HttpRequest&, const zephyr::context::Context&) {
        return ex::schedule(pool_scheduler)
            | ex::then([]() {
                std::cout << "[Handler] Executing async work...\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return zephyr::http::HttpResponse::ok("Async response after 100ms");
            });
    });
    
    // POST endpoint
    http_router.post("/echo", [](const zephyr::http::HttpRequest& req, const zephyr::context::Context&) {
        return zephyr::http::HttpResponse::ok("Echo: " + req.body);
    });
    
    // ========================================================================
    // Konfiguracja UDP Router
    // ========================================================================
    
    UdpRouter udp_router;
    
    // Handler dla portu 5000
    udp_router.on_port(5000, [](const zephyr::udp::UdpPacket& packet, const zephyr::context::Context&) {
        std::cout << "[UDP Handler 5000] Received " << packet.data.size() 
                 << " bytes\n";
        
        // Echo - zwracamy te same dane
        return std::optional<std::vector<uint8_t>>{packet.data};
    });
    
    // Handler dla portu 5001 - jednostronny, bez odpowiedzi
    udp_router.on_port(5001, [](const zephyr::udp::UdpPacket& packet, const zephyr::context::Context&) {
        std::cout << "[UDP Handler 5001] Logging packet from " 
                 << packet.source_ip << "\n";
        // Brak odpowiedzi
        return std::optional<std::vector<uint8_t>>{};
    });
    
    // ========================================================================
    // Uruchomienie serwerów
    // ========================================================================
    
    auto io_ctx = std::make_shared<zephyr::io::IoUringContext>();

    TcpServer tcp_server(pool_scheduler, http_router, io_ctx);
    UdpServer udp_server(pool_scheduler, udp_router);
    
    if (!tcp_server.listen(8080)) {
        std::cerr << "Failed to start TCP server\n";
        return 1;
    }
    
    if (!udp_server.bind(5000)) {
        std::cerr << "Failed to start UDP server\n";
        return 1;
    }
    
    tcp_server.run();
    udp_server.run();
    
    std::cout << "\n=== Serwery uruchomione ===\n";
    std::cout << "TCP (HTTP) na porcie 8080\n";
    std::cout << "UDP na porcie 5000\n\n";
    
    std::cout << "Możesz testować przez:\n";
    std::cout << "  curl http://localhost:8080/\n";
    std::cout << "  curl http://localhost:8080/users/123\n";
    std::cout << "  curl http://localhost:8080/async\n";
    std::cout << "  curl -X POST http://localhost:8080/echo -d 'Hello World'\n";
    std::cout << "  echo 'test' | nc -u localhost 5000\n\n";
    
    // Serwery działają w tle na pulę wątków
    // Główny wątek czeka na Ctrl+C
    std::cout << "Naciśnij Ctrl+C aby zatrzymać...\n\n";
    
    // Symulujemy działanie serwera przez 60 sekund
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    std::cout << "\n=== Zatrzymywanie serwerów ===\n";
    tcp_server.stop();
    udp_server.stop();
    
    // Czekamy chwilę na zakończenie pending operations
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "\n=== KLUCZOWE PUNKTY IMPLEMENTACJI ===\n\n";
    
    std::cout << "1. EXEC I STRANDY:\n";
    std::cout << "   - Używamy exec::static_thread_pool(4) dla 4 wątków roboczych\n";
    std::cout << "   - Operacje sesji TCP planowane na schedulerze puli wątków\n";
    std::cout << "   - Różne sesje mogą działać równolegle, kolejność w ramach sesji zapewnia logika handlera\n\n";
    
    std::cout << "2. KOMPOZYCJA SENDERÓW:\n";
    std::cout << "   - Czytanie -> Parsowanie -> Routing -> Handler -> Serializacja -> Zapis\n";
    std::cout << "   - Każdy krok to sender komponowany przez operatory | ex::then, | ex::let_value\n";
    std::cout << "   - Błędy propagują przez | ex::upon_error\n";
    std::cout << "   - Wszystko asynchroniczne bez blokowania wątków\n\n";
    
    std::cout << "3. HTTP ROUTER:\n";
    std::cout << "   - Pattern matching: /users/:id automatycznie wyciąga parametry\n";
    std::cout << "   - Synchroniczne handlery: return HttpResponse::ok(...)\n";
    std::cout << "   - Asynchroniczne handlery: return ex::schedule(...) | ex::then(...)\n";
    std::cout << "   - Router sam jest senderem: router.route(req) zwraca sendera\n\n";
    
    std::cout << "4. UDP ROUTER:\n";
    std::cout << "   - Routing po porcie docelowym\n";
    std::cout << "   - Handler może zwrócić odpowiedź lub std::nullopt (brak odpowiedzi)\n";
    std::cout << "   - Każdy pakiet obsługiwany w swoim strandzie\n\n";
    
    std::cout << "5. BRAK MUTEXÓW W SESJI:\n";
    std::cout << "   - TcpSession::receive_buffer modyfikowany bez locków\n";
    std::cout << "   - Bezpieczne dzięki strand scheduler\n";
    std::cout << "   - Wszystkie operacje sesji wykonują się sekwencyjnie\n\n";
    
    std::cout << "6. UPROSZCZENIA W TYM PRZYKŁADZIE:\n";
    std::cout << "   - Używamy recv/send z MSG_DONTWAIT zamiast io_uring\n";
    std::cout << "   - Sleep między iteracjami accept/receive (w produkcji: epoll/io_uring)\n";
    std::cout << "   - Prosty parser HTTP (w produkcji: llhttp lub http-parser)\n\n";
    
    std::cout << "7. CO ZMIENIĆ DLA PRODUKCJI:\n";
    std::cout << "   - Zamień recv/send na io_uring async operacje\n";
    std::cout << "   - Dodaj prawdziwy parser HTTP (llhttp)\n";
    std::cout << "   - Dodaj obsługę Keep-Alive\n";
    std::cout << "   - Dodaj rate limiting, timeouty\n";
    std::cout << "   - Dodaj proper logging (spdlog)\n";
    std::cout << "   - Dodaj metryki (prometheus)\n\n";
    
    std::cout << "8. DODAWANIE NOWYCH PROTOKOŁÓW:\n";
    std::cout << "   - WebSocket: Specjalny route który upgrade'uje połączenie\n";
    std::cout << "   - gRPC: Parser protobuf + routing po service/method\n";
    std::cout << "   - Custom protocol: Własny parser + router\n";
    std::cout << "   - Wszystko komponuje się przez sendery!\n\n";
    
    std::cout << "9. MIDDLEWARE:\n";
    std::cout << "   - Można dodać warstwę między routerem a handlerem\n";
    std::cout << "   - Auth middleware: sprawdza token przed handlerem\n";
    std::cout << "   - Logging middleware: loguje request/response\n";
    std::cout << "   - Rate limiting: sprawdza limity w Redis\n";
    std::cout << "   - Każdy middleware to sender!\n\n";
    
    std::cout << "10. INTEGRACJA Z BAZĄ DANYCH:\n";
    std::cout << "    - Używaj async driver zwracającego sendery\n";
    std::cout << "    - PostgreSQL: używaj libpqxx z async API\n";
    std::cout << "    - MongoDB: async driver\n";
    std::cout << "    - Redis: async driver\n";
    std::cout << "    - Komponuj zapytania DB w handlerze:\n";
    std::cout << "      return db.query(...) | ex::then([](result) { ... });\n\n";
    
    std::cout << "=== PRZYKŁAD ROZSZERZEŃ ===\n\n";
    
    std::cout << "Dodawanie middleware auth:\n";
    std::cout << "  auto with_auth = [router](HttpRequest req) {\n";
    std::cout << "    if (req.headers[\"Authorization\"] != \"Bearer token\") {\n";
    std::cout << "      HttpResponse err; err.status_code = 401;\n";
    std::cout << "      return ex::just(err);\n";
    std::cout << "    }\n";
    std::cout << "    return router.route(req);\n";
    std::cout << "  };\n\n";
    
    std::cout << "Dodawanie WebSocket:\n";
    std::cout << "  http_router.get(\"/ws\", [](auto req, auto ctx) {\n";
    std::cout << "    if (req.headers[\"Upgrade\"] == \"websocket\") {\n";
    std::cout << "      // Upgrade connection to WebSocket\n";
    std::cout << "      return handle_websocket_upgrade(req);\n";
    std::cout << "    }\n";
    std::cout << "    return HttpResponse::not_found();\n";
    std::cout << "  });\n\n";
    
    std::cout << "Async handler z bazą danych:\n";
    std::cout << "  http_router.get_async(\"/data\", [db](auto req, auto ctx) {\n";
    std::cout << "    return db.query(\"SELECT * FROM table\")\n";
    std::cout << "      | ex::then([](auto result) {\n";
    std::cout << "          return HttpResponse::json(to_json(result));\n";
    std::cout << "        });\n";
    std::cout << "  });\n\n";
    
    std::cout << "=== KONIEC ===\n";
    
    return 0;
}