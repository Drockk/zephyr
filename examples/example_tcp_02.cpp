// Kompiluj: g++ -std=c++23 -o server server.cpp -lpthread
// Wymaga: stdexec + exec z repozytorium NVIDIA/stdexec

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/inline_scheduler.hpp>
#include <exec/any_sender_of.hpp>
#include <exec/single_thread_context.hpp>

#include <liburing.h>

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
#include <queue>
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

namespace ex = stdexec;
template<class... Sigs>
using any_sender_of = exec::any_receiver_ref<ex::completion_signatures<Sigs...>>::template any_sender<>;

// ============================================================================
// IO_URING CONTEXT - minimalne opakowanie
// ============================================================================

class IoUringContext {
public:
    explicit IoUringContext(unsigned entries = 256) {
        if (io_uring_queue_init(entries, &ring_, 0) != 0) {
            throw std::runtime_error("io_uring_queue_init failed");
        }
    }

    ~IoUringContext() {
        io_uring_queue_exit(&ring_);
    }

    int accept(int listen_fd) {
        sockaddr_in addr{};
        socklen_t addrlen = sizeof(addr);
        io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) return -1;
        io_uring_prep_accept(sqe, listen_fd, (sockaddr*)&addr, &addrlen, SOCK_NONBLOCK);
        io_uring_submit_and_wait(&ring_, 1);
        io_uring_cqe* cqe = nullptr;
        if (io_uring_wait_cqe(&ring_, &cqe) != 0) return -1;
        int res = cqe->res;
        io_uring_cqe_seen(&ring_, cqe);
        return res;
    }

    ssize_t recv(int fd, void* buf, size_t len) {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) return -1;
        io_uring_prep_recv(sqe, fd, buf, len, 0);
        io_uring_submit_and_wait(&ring_, 1);
        io_uring_cqe* cqe = nullptr;
        if (io_uring_wait_cqe(&ring_, &cqe) != 0) return -1;
        ssize_t res = cqe->res;
        io_uring_cqe_seen(&ring_, cqe);
        return res;
    }

    ssize_t send(int fd, const void* buf, size_t len) {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) return -1;
        io_uring_prep_send(sqe, fd, buf, len, 0);
        io_uring_submit_and_wait(&ring_, 1);
        io_uring_cqe* cqe = nullptr;
        if (io_uring_wait_cqe(&ring_, &cqe) != 0) return -1;
        ssize_t res = cqe->res;
        io_uring_cqe_seen(&ring_, cqe);
        return res;
    }

private:
    io_uring ring_{};
};

// ============================================================================
// TYPY PODSTAWOWE - Struktury danych dla HTTP i UDP
// ============================================================================

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> path_params;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::map<std::string, std::string> headers;
    std::string body;
    
    static HttpResponse ok(std::string body_text) {
        HttpResponse r;
        r.body = std::move(body_text);
        return r;
    }
    
    static HttpResponse json(std::string json_text) {
        HttpResponse r;
        r.headers["Content-Type"] = "application/json";
        r.body = std::move(json_text);
        return r;
    }
    
    static HttpResponse not_found() {
        HttpResponse r;
        r.status_code = 404;
        r.status_text = "Not Found";
        r.body = "404 Not Found";
        return r;
    }
};

struct UdpPacket {
    std::string source_ip;
    int source_port;
    int dest_port;
    std::vector<uint8_t> data;
};

// ============================================================================
// HTTP PARSER I SERIALIZER
// ============================================================================

class HttpParser {
public:
    static std::optional<HttpRequest> parse(const std::string& raw) {
        HttpRequest req;
        
        auto first_line_end = raw.find("\r\n");
        if (first_line_end == std::string::npos) return std::nullopt;
        
        std::string request_line = raw.substr(0, first_line_end);
        size_t method_end = request_line.find(' ');
        if (method_end == std::string::npos) return std::nullopt;
        
        req.method = request_line.substr(0, method_end);
        size_t path_start = method_end + 1;
        size_t path_end = request_line.find(' ', path_start);
        if (path_end == std::string::npos) return std::nullopt;
        
        req.path = request_line.substr(path_start, path_end - path_start);
        req.version = request_line.substr(path_end + 1);
        
        size_t pos = first_line_end + 2;
        while (true) {
            auto line_end = raw.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            
            std::string line = raw.substr(pos, line_end - pos);
            if (line.empty()) {
                pos = line_end + 2;
                break;
            }
            
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                req.headers[name] = value;
            }
            pos = line_end + 2;
        }
        
        if (pos < raw.size()) {
            req.body = raw.substr(pos);
        }
        
        return req;
    }
    
    static bool is_complete(const std::string& data) {
        return data.find("\r\n\r\n") != std::string::npos;
    }
};

class HttpSerializer {
public:
    static std::string serialize(const HttpResponse& resp) {
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
// CONTEXT - Współdzielone zasoby
// ============================================================================

class Context {
    std::map<std::string, std::shared_ptr<void>> resources;
public:
    template<typename T>
    void set(const std::string& name, std::shared_ptr<T> resource) {
        resources[name] = resource;
    }
    
    template<typename T>
    std::shared_ptr<T> get(const std::string& name) const {
        auto it = resources.find(name);
        if (it == resources.end()) return nullptr;
        return std::static_pointer_cast<T>(it->second);
    }
};

// ============================================================================
// HTTP ROUTER - Pattern matching i routing
// ============================================================================

class HttpRoute {
public:
    using HttpSender = any_sender_of<
        ex::set_value_t(HttpResponse),
        ex::set_error_t(std::exception_ptr),
        ex::set_stopped_t()
    >;
    using Handler = std::function<HttpSender(const HttpRequest&, const Context&)>;
    
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
    
    HttpSender invoke(HttpRequest req, const Context& ctx) const {
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
    std::shared_ptr<Context> context;
    
public:
    HttpRouter() : context(std::make_shared<Context>()) {}
    
    template<typename T>
    void add_resource(const std::string& name, std::shared_ptr<T> res) {
        context->set(name, res);
    }
    
    // Synchroniczne handlery
    template<typename SyncHandler>
    void add_route(std::string method, std::string path, SyncHandler handler) {
        auto async_handler = [h = std::move(handler)]
            (const HttpRequest& req, const Context& ctx) {
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
            [h = std::move(handler)](const HttpRequest& req, const Context& ctx) {
                return HttpSender{h(req, ctx)};
            }
        );
    }
    
    HttpSender route(HttpRequest req) const {
        for (const auto& r : routes) {
            if (r.matches(req.method, req.path)) {
                return r.invoke(std::move(req), *context);
            }
        }
        return HttpSender{ex::just(HttpResponse::not_found())};
    }
};

// ============================================================================
// UDP ROUTER - Routing po portach
// ============================================================================

class UdpRouter {
public:
    using UdpSender = any_sender_of<
        ex::set_value_t(std::optional<std::vector<uint8_t>>),
        ex::set_error_t(std::exception_ptr),
        ex::set_stopped_t()
    >;
    using Handler = std::function<UdpSender(const UdpPacket&, const Context&)>;
    
private:
    struct Route {
        int port;
        Handler handler;
    };
    
    std::vector<Route> routes;
    std::shared_ptr<Context> context;
    
public:
    UdpRouter() : context(std::make_shared<Context>()) {}
    
    template<typename SyncHandler>
    void on_port(int port, SyncHandler handler) {
        auto async_handler = [h = std::move(handler)]
            (const UdpPacket& packet, const Context& ctx) {
            return UdpSender{ex::just(h(packet, ctx))};
        };
        routes.push_back(Route{port, std::move(async_handler)});
    }
    
    UdpSender route(UdpPacket packet) const {
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
struct StrandState : std::enable_shared_from_this<StrandState<BaseScheduler>> {
    BaseScheduler base;
    std::mutex m;
    std::queue<std::function<void()>> tasks;
    bool active = false;

    explicit StrandState(BaseScheduler b) : base(std::move(b)) {}

    void enqueue(std::function<void()> fn) {
        bool start = false;
        {
            std::lock_guard<std::mutex> lock(m);
            tasks.push(std::move(fn));
            if (!active) {
                active = true;
                start = true;
            }
        }
        if (start) {
            dispatch();
        }
    }

    void dispatch() {
        auto self = this->shared_from_this();
        ex::start_detached(
            ex::schedule(base)
            | ex::then([self]() { self->run_one(); })
        );
    }

    void run_one() {
        std::function<void()> fn;
        {
            std::lock_guard<std::mutex> lock(m);
            if (tasks.empty()) {
                active = false;
                return;
            }
            fn = std::move(tasks.front());
            tasks.pop();
        }
        try {
            fn();
        } catch (...) {
            // swallow; errors are delivered to receiver
        }
        dispatch();
    }
};

template<typename BaseScheduler, class Receiver>
struct StrandOp {
    std::shared_ptr<StrandState<BaseScheduler>> st;
    Receiver rcv;
    void start() noexcept {
        auto self = st;
        st->enqueue([self, r = std::move(rcv)]() mutable {
            try {
                ex::set_value(std::move(r));
            } catch (...) {
                ex::set_error(std::move(r), std::current_exception());
            }
        });
    }
};

template<typename BaseScheduler>
struct StrandSender {
    std::shared_ptr<StrandState<BaseScheduler>> st;
    using completion_signatures = ex::completion_signatures<
        ex::set_value_t(),
        ex::set_error_t(std::exception_ptr),
        ex::set_stopped_t()
    >;

    template<class Receiver>
    StrandOp<BaseScheduler, Receiver> connect(Receiver r) const {
        return StrandOp<BaseScheduler, Receiver>{st, std::move(r)};
    }
};

template<typename BaseScheduler>
class StrandScheduler {
public:
    explicit StrandScheduler(BaseScheduler base)
        : state_(std::make_shared<StrandState<BaseScheduler>>(std::move(base))) {}

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
    std::shared_ptr<StrandState<BaseScheduler>> state_;
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
    std::shared_ptr<IoUringContext> io_ctx;
    
    std::string receive_buffer;
    bool is_active = true;
    
public:
    TcpSession(int fd, const HttpRouter& r, std::shared_ptr<IoUringContext> io)
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
                ssize_t n = self->io_ctx->recv(self->socket_fd, buffer, sizeof(buffer));
                if (n > 0) {
                    return std::string(buffer, n);
                } else if (n == 0) {
                    throw std::runtime_error("Connection closed");
                } else {
                    throw std::runtime_error("Read error");
                }
            })
            | ex::then([self](std::string data) -> std::optional<HttpRequest> {
                if (data.empty()) return std::nullopt;
                
                self->receive_buffer += data;
                std::cout << "[TCP Session " << self->socket_fd << "] Received " 
                         << data.size() << " bytes\n";
                
                if (HttpParser::is_complete(self->receive_buffer)) {
                    auto req = HttpParser::parse(self->receive_buffer);
                    self->receive_buffer.clear();
                    return req;
                }
                return std::nullopt;
            })
            | ex::let_value([self](std::optional<HttpRequest> maybe_req) {
                using ResponseSender = any_sender_of<
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
                        | ex::then([](HttpResponse resp) {
                            return HttpSerializer::serialize(resp);
                        })
                };
            })
            | ex::then([self](std::string response) {
                if (!response.empty()) {
                    ssize_t sent = self->io_ctx->send(self->socket_fd, response.data(), 
                                       response.size());
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
    std::shared_ptr<IoUringContext> io_ctx;

    using Session = TcpSession;
    std::map<int, std::shared_ptr<Session>> sessions;
    
public:
    TcpServer(BaseScheduler sched, const HttpRouter& r, std::shared_ptr<IoUringContext> io)
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
            | ex::then([this]() -> std::optional<UdpPacket> {
                char buffer[65536];
                sockaddr_in client_addr{};
                socklen_t addr_len = sizeof(client_addr);
                
                ssize_t n = recvfrom(socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT,
                                    (sockaddr*)&client_addr, &addr_len);
                
                if (n > 0) {
                    UdpPacket packet;
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
            | ex::let_value([this](std::optional<UdpPacket> maybe_packet) {
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
    http_router.get("/", [](const HttpRequest&, const Context&) {
        return HttpResponse::ok("Welcome to the server!");
    });
    
    // Endpoint z parametrami
    http_router.get("/users/:id", [](const HttpRequest& req, const Context&) {
        std::string user_id = req.path_params.at("id");
        std::string json = R"({"id": ")" + user_id + R"(", "name": "User )" + user_id + R"("})";
        return HttpResponse::json(json);
    });
    
    // Asynchroniczny endpoint - symuluje zapytanie do bazy
    http_router.get_async("/async", [pool_scheduler](const HttpRequest&, const Context&) {
        return ex::schedule(pool_scheduler)
            | ex::then([]() {
                std::cout << "[Handler] Executing async work...\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return HttpResponse::ok("Async response after 100ms");
            });
    });
    
    // POST endpoint
    http_router.post("/echo", [](const HttpRequest& req, const Context&) {
        return HttpResponse::ok("Echo: " + req.body);
    });
    
    // ========================================================================
    // Konfiguracja UDP Router
    // ========================================================================
    
    UdpRouter udp_router;
    
    // Handler dla portu 5000
    udp_router.on_port(5000, [](const UdpPacket& packet, const Context&) {
        std::cout << "[UDP Handler 5000] Received " << packet.data.size() 
                 << " bytes\n";
        
        // Echo - zwracamy te same dane
        return std::optional<std::vector<uint8_t>>{packet.data};
    });
    
    // Handler dla portu 5001 - jednostronny, bez odpowiedzi
    udp_router.on_port(5001, [](const UdpPacket& packet, const Context&) {
        std::cout << "[UDP Handler 5001] Logging packet from " 
                 << packet.source_ip << "\n";
        // Brak odpowiedzi
        return std::optional<std::vector<uint8_t>>{};
    });
    
    // ========================================================================
    // Uruchomienie serwerów
    // ========================================================================
    
    auto io_ctx = std::make_shared<IoUringContext>();

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