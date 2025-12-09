// Kompiluj: g++ -std=c++23 -o server server.cpp -lpthread
// Wymaga: stdexec + exec z repozytorium NVIDIA/stdexec

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

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
#include <functional>

namespace ex = stdexec;

// ============================================================================
// TYPY PODSTAWOWE
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
// HTTP ROUTER - Uproszczona wersja
// ============================================================================

class HttpRouter {
public:
    using SyncHandler = std::function<HttpResponse(const HttpRequest&)>;
    
private:
    struct Route {
        std::string method;
        std::regex path_regex;
        std::vector<std::string> param_names;
        SyncHandler handler;
        
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
    };
    
    std::vector<Route> routes;
    
    void compile_pattern(const std::string& pattern, Route& route) {
        std::string regex_pattern;
        size_t pos = 0;
        
        while (pos < pattern.size()) {
            if (pattern[pos] == ':') {
                size_t end = pattern.find('/', pos);
                if (end == std::string::npos) end = pattern.size();
                
                std::string param_name = pattern.substr(pos + 1, end - pos - 1);
                route.param_names.push_back(param_name);
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
        route.path_regex = std::regex(regex_pattern);
    }
    
public:
    void add_route(std::string method, std::string path, SyncHandler handler) {
        Route route;
        route.method = std::move(method);
        route.handler = std::move(handler);
        compile_pattern(path, route);
        routes.push_back(std::move(route));
    }
    
    void get(std::string path, SyncHandler handler) {
        add_route("GET", std::move(path), std::move(handler));
    }
    
    void post(std::string path, SyncHandler handler) {
        add_route("POST", std::move(path), std::move(handler));
    }
    
    HttpResponse route(HttpRequest req) const {
        for (const auto& r : routes) {
            if (r.matches(req.method, req.path)) {
                req.path_params = r.extract_params(req.path);
                return r.handler(req);
            }
        }
        return HttpResponse::not_found();
    }
};

// ============================================================================
// TCP SESSION
// ============================================================================

template<typename Scheduler>
class TcpSession : public std::enable_shared_from_this<TcpSession<Scheduler>> {
    int socket_fd;
    Scheduler sched;
    const HttpRouter& router;
    
    std::string receive_buffer;
    bool is_active = true;
    
public:
    TcpSession(int fd, Scheduler s, const HttpRouter& r)
        : socket_fd(fd), sched(s), router(r) {
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
        
        auto work = ex::schedule(sched)
            | ex::then([self]() -> std::optional<HttpRequest> {
                // Czytanie z socketu
                char buffer[4096];
                ssize_t n = recv(self->socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
                
                if (n > 0) {
                    self->receive_buffer += std::string(buffer, n);
                    std::cout << "[TCP Session " << self->socket_fd << "] Received " 
                             << n << " bytes\n";
                    
                    if (HttpParser::is_complete(self->receive_buffer)) {
                        auto req = HttpParser::parse(self->receive_buffer);
                        self->receive_buffer.clear();
                        return req;
                    }
                } else if (n == 0) {
                    throw std::runtime_error("Connection closed");
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    throw std::runtime_error("Read error");
                }
                
                return std::nullopt;
            })
            | ex::then([self](std::optional<HttpRequest> maybe_req) -> std::string {
                if (!maybe_req) {
                    return ""; // Niepełne żądanie - kontynuuj
                }
                
                std::cout << "[TCP Session " << self->socket_fd << "] Complete request: "
                         << maybe_req->method << " " << maybe_req->path << "\n";
                
                // Routuj i przetwórz
                HttpResponse resp = self->router.route(*maybe_req);
                return HttpSerializer::serialize(resp);
            })
            | ex::then([self](std::string response) {
                if (!response.empty()) {
                    ssize_t sent = send(self->socket_fd, response.data(), 
                                       response.size(), MSG_NOSIGNAL);
                    
                    std::cout << "[TCP Session " << self->socket_fd << "] Sent " 
                             << sent << " bytes\n";
                }
                
                // Kontynuuj obsługę kolejnych żądań
                if (self->is_active) {
                    // Małe opóźnienie między iteracjami
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
// TCP SERVER
// ============================================================================

template<typename Scheduler>
class TcpServer {
    int listen_socket = -1;
    Scheduler pool_scheduler;
    const HttpRouter& router;
    bool is_running = true;
    
    std::map<int, std::shared_ptr<TcpSession<Scheduler>>> sessions;
    
public:
    TcpServer(Scheduler sched, const HttpRouter& r)
        : pool_scheduler(sched), router(r) {}
    
    ~TcpServer() {
        stop();
    }
    
    bool listen(int port) {
        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket < 0) return false;
        
        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
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
                sockaddr_in client_addr{};
                socklen_t addr_len = sizeof(client_addr);
                
                int client_fd = accept(listen_socket, (sockaddr*)&client_addr, &addr_len);
                
                if (client_fd >= 0) {
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);
                    
                    std::cout << "[TCP Server] Accepted connection: fd=" << client_fd << "\n";
                    
                    auto session = std::make_shared<TcpSession<Scheduler>>(
                        client_fd, pool_scheduler, router
                    );
                    
                    sessions[client_fd] = session;
                    session->start();
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "[TCP Server] Accept error: " << strerror(errno) << "\n";
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
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
// PRZYKŁAD UŻYCIA
// ============================================================================

int main() {
    std::cout << "=== Uproszczony TCP/HTTP Server z exec ===\n\n";
    
    // Pula 4 wątków roboczych
    exec::static_thread_pool pool(4);
    auto pool_scheduler = pool.get_scheduler();
    
    // ========================================================================
    // Konfiguracja HTTP Router
    // ========================================================================
    
    HttpRouter http_router;
    
    // Prosty endpoint
    http_router.get("/", [](const HttpRequest&) {
        return HttpResponse::ok("Welcome to the server!");
    });
    
    // Endpoint z parametrami
    http_router.get("/users/:id", [](const HttpRequest& req) {
        std::string user_id = req.path_params.at("id");
        std::string json = R"({"id": ")" + user_id + R"(", "name": "User )" + user_id + R"("})";
        return HttpResponse::json(json);
    });
    
    // Endpoint z "asynchroniczną" pracą (w ramach handlera)
    http_router.get("/async", [](const HttpRequest&) {
        std::cout << "[Handler] Executing work in handler...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return HttpResponse::ok("Response after 100ms");
    });
    
    // POST endpoint
    http_router.post("/echo", [](const HttpRequest& req) {
        return HttpResponse::ok("Echo: " + req.body);
    });
    
    // ========================================================================
    // Uruchomienie serwera
    // ========================================================================
    
    TcpServer tcp_server(pool_scheduler, http_router);
    
    if (!tcp_server.listen(8080)) {
        std::cerr << "Failed to start TCP server\n";
        return 1;
    }
    
    tcp_server.run();
    
    std::cout << "\n=== Serwer uruchomiony ===\n";
    std::cout << "TCP (HTTP) na porcie 8080\n\n";
    
    std::cout << "Możesz testować przez:\n";
    std::cout << "  curl http://localhost:8080/\n";
    std::cout << "  curl http://localhost:8080/users/123\n";
    std::cout << "  curl http://localhost:8080/async\n";
    std::cout << "  curl -X POST http://localhost:8080/echo -d 'Hello World'\n\n";
    
    std::cout << "Naciśnij Ctrl+C aby zatrzymać...\n\n";
    
    // Symulujemy działanie serwera przez 60 sekund
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    std::cout << "\n=== Zatrzymywanie serwera ===\n";
    tcp_server.stop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "\n=== KLUCZOWE PUNKTY IMPLEMENTACJI ===\n\n";
    
    std::cout << "1. EXEC I SCHEDULERY:\n";
    std::cout << "   - Używamy exec::static_thread_pool(4) dla 4 wątków roboczych\n";
    std::cout << "   - Każda sesja TCP wykonuje się na pool_scheduler\n";
    std::cout << "   - Różne sesje mogą działać równolegle\n\n";
    
    std::cout << "2. KOMPOZYCJA SENDERÓW:\n";
    std::cout << "   - Czytanie -> Parsowanie -> Routing -> Serializacja -> Zapis\n";
    std::cout << "   - Każdy krok to sender komponowany przez | ex::then\n";
    std::cout << "   - Błędy propagują przez | ex::upon_error\n\n";
    
    std::cout << "3. HTTP ROUTER:\n";
    std::cout << "   - Pattern matching: /users/:id automatycznie wyciąga parametry\n";
    std::cout << "   - Synchroniczne handlery: return HttpResponse::ok(...)\n";
    std::cout << "   - Każdy handler jest std::function<HttpResponse(HttpRequest)>\n\n";
    
    std::cout << "4. UPROSZCZENIA:\n";
    std::cout << "   - Używamy recv/send z MSG_DONTWAIT zamiast io_uring\n";
    std::cout << "   - Sleep między iteracjami accept/receive\n";
    std::cout << "   - Prosty parser HTTP\n";
    std::cout << "   - Brak true strand scheduler - każda sesja na pool\n\n";
    
    std::cout << "5. DLACZEGO TA WERSJA DZIAŁA:\n";
    std::cout << "   - Handler zwraca HttpResponse bezpośrednio, nie sendera\n";
    std::cout << "   - std::function może przechowywać takie callable\n";
    std::cout << "   - Asynchroniczność jest na poziomie senderów w sesji\n";
    std::cout << "   - Router sam jest synchroniczny i szybki\n\n";
    
    std::cout << "6. CO ZMIENIĆ DLA PRODUKCJI:\n";
    std::cout << "   - Zamień recv/send na io_uring async operacje\n";
    std::cout << "   - Dodaj prawdziwy parser HTTP (llhttp)\n";
    std::cout << "   - Dodaj obsługę Keep-Alive\n";
    std::cout << "   - Dodaj timeouty, rate limiting\n";
    std::cout << "   - Dodaj proper logging (spdlog)\n\n";
    
    return 0;
}