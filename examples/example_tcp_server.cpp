/**
 * @file example_tcp_server.cpp
 * @brief Standalone TCP server example with epoll and two-thread architecture
 *
 * Features:
 * - Asynchronous I/O using epoll
 * - IO thread: handles epoll events and socket operations
 * - Business logic thread: processes messages and generates responses
 * - Thread-safe message queue for communication between threads
 * - Echo server that modifies received messages before sending back
 *
 * Compile: g++ -std=c++20 -pthread -o tcp_server example_tcp_server.cpp
 */

#include "zephyr/core/application.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <zephyr/plugin/details/pluginConcept.hpp>

constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 64;
constexpr int BUFFER_SIZE = 4096;

// ============================================================================
// Thread-safe message queue
// ============================================================================
template <typename T>
class ThreadSafeQueue
{
public:
    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    bool try_pop(T& item)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void wait_and_pop(T& item)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        if (!queue_.empty()) {
            item = std::move(queue_.front());
            queue_.pop();
        }
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

    bool is_stopped() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopped_ && queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable cv_;
    bool stopped_ = false;
};

// ============================================================================
// Message types for inter-thread communication
// ============================================================================
struct IncomingMessage
{
    int client_fd;
    std::string data;
};

struct OutgoingMessage
{
    int client_fd;
    std::string data;
};

// ============================================================================
// Utility functions
// ============================================================================
bool set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

int create_server_socket(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket\n";
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Failed to set SO_REUSEADDR\n";
        close(server_fd);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "Failed to bind to port " << port << "\n";
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, SOMAXCONN) == -1) {
        std::cerr << "Failed to listen\n";
        close(server_fd);
        return -1;
    }

    if (!set_non_blocking(server_fd)) {
        std::cerr << "Failed to set non-blocking\n";
        close(server_fd);
        return -1;
    }

    return server_fd;
}

// ============================================================================
// TCP Server with epoll
// ============================================================================
class TcpServer
{
public:
    TcpServer(int port) : port_(port) {}

    ~TcpServer()
    {
        stop();
    }

    bool start()
    {
        server_fd_ = create_server_socket(port_);
        if (server_fd_ == -1) {
            return false;
        }

        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1) {
            std::cerr << "Failed to create epoll\n";
            close(server_fd_);
            return false;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = server_fd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev) == -1) {
            std::cerr << "Failed to add server socket to epoll\n";
            close(epoll_fd_);
            close(server_fd_);
            return false;
        }

        running_ = true;

        // Start business logic thread
        business_thread_ = std::thread(&TcpServer::business_logic_loop, this);

        // Start I/O thread
        io_thread_ = std::thread(&TcpServer::io_loop, this);

        std::cout << "Server started on port " << port_ << "\n";
        std::cout << "IO thread and Business Logic thread running\n";

        return true;
    }

    void stop()
    {
        if (!running_) {
            return;
        }

        running_ = false;
        incoming_queue_.stop();
        outgoing_queue_.stop();

        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        if (business_thread_.joinable()) {
            business_thread_.join();
        }

        // Close all client connections
        for (auto& [fd, _] : client_buffers_) {
            close(fd);
        }
        client_buffers_.clear();

        if (epoll_fd_ != -1) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
        if (server_fd_ != -1) {
            close(server_fd_);
            server_fd_ = -1;
        }

        std::cout << "Server stopped\n";
    }

    void wait()
    {
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        if (business_thread_.joinable()) {
            business_thread_.join();
        }
    }

private:
    // I/O thread: handles epoll events
    void io_loop()
    {
        std::cout << "[IO Thread] Started\n";

        std::vector<epoll_event> events(MAX_EVENTS);

        while (running_) {
            // Process outgoing messages first
            process_outgoing_messages();

            int nfds = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, 100);
            if (nfds == -1) {
                if (errno == EINTR)
                    continue;
                std::cerr << "[IO Thread] epoll_wait error: " << strerror(errno) << "\n";
                break;
            }

            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == server_fd_) {
                    handle_new_connection();
                } else {
                    handle_client_event(events[i]);
                }
            }
        }

        std::cout << "[IO Thread] Stopped\n";
    }

    void handle_new_connection()
    {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // No more connections
                }
                std::cerr << "[IO Thread] Accept error: " << strerror(errno) << "\n";
                break;
            }

            if (!set_non_blocking(client_fd)) {
                std::cerr << "[IO Thread] Failed to set client non-blocking\n";
                close(client_fd);
                continue;
            }

            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLET;  // Edge-triggered
            ev.data.fd = client_fd;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                std::cerr << "[IO Thread] Failed to add client to epoll\n";
                close(client_fd);
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                client_buffers_[client_fd] = "";
            }

            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
            std::cout << "[IO Thread] New connection from " << ip_str << ":" << ntohs(client_addr.sin_port)
                      << " (fd=" << client_fd << ")\n";
        }
    }

    void handle_client_event(const epoll_event& ev)
    {
        int client_fd = ev.data.fd;

        if (ev.events & (EPOLLERR | EPOLLHUP)) {
            close_client(client_fd);
            return;
        }

        if (ev.events & EPOLLIN) {
            handle_client_read(client_fd);
        }
    }

    void handle_client_read(int client_fd)
    {
        char buffer[BUFFER_SIZE];

        while (true) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

            if (bytes_read > 0) {
                std::string data(buffer, bytes_read);

                // Send to business logic thread
                incoming_queue_.push({client_fd, data});
                std::cout << "[IO Thread] Received " << bytes_read << " bytes from fd=" << client_fd << "\n";

            } else if (bytes_read == 0) {
                // Client disconnected
                close_client(client_fd);
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // No more data
                }
                std::cerr << "[IO Thread] Read error: " << strerror(errno) << "\n";
                close_client(client_fd);
                return;
            }
        }
    }

    void process_outgoing_messages()
    {
        OutgoingMessage msg;
        while (outgoing_queue_.try_pop(msg)) {
            ssize_t total_sent = 0;
            ssize_t to_send = msg.data.size();

            while (total_sent < to_send) {
                ssize_t sent = send(msg.client_fd, msg.data.data() + total_sent, to_send - total_sent, MSG_NOSIGNAL);
                if (sent == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Would block, try again later
                        // In production, we'd buffer and use EPOLLOUT
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    std::cerr << "[IO Thread] Send error: " << strerror(errno) << "\n";
                    break;
                }
                total_sent += sent;
            }

            if (total_sent == to_send) {
                std::cout << "[IO Thread] Sent " << total_sent << " bytes to fd=" << msg.client_fd << "\n";
            }
        }
    }

    void close_client(int client_fd)
    {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
        close(client_fd);

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_buffers_.erase(client_fd);
        }

        std::cout << "[IO Thread] Client disconnected (fd=" << client_fd << ")\n";
    }

    // Business logic thread: processes messages
    void business_logic_loop()
    {
        std::cout << "[Business Thread] Started\n";

        while (running_ || !incoming_queue_.is_stopped()) {
            IncomingMessage msg;
            incoming_queue_.wait_and_pop(msg);

            if (msg.data.empty() && !running_) {
                break;
            }

            // Process the message - modify it
            std::string response = process_message(msg.client_fd, msg.data);

            // Send response back through IO thread
            if (!response.empty()) {
                outgoing_queue_.push({msg.client_fd, response});
            }
        }

        std::cout << "[Business Thread] Stopped\n";
    }

    std::string process_message(int client_fd, const std::string& data)
    {
        std::cout << "[Business Thread] Processing message from fd=" << client_fd << ": \"" << data << "\"\n";

        // Business logic: transform the message
        // Example: convert to uppercase and add prefix
        std::string response = "[SERVER ECHO] ";
        for (char c : data) {
            response += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        std::cout << "[Business Thread] Generated response: \"" << response << "\"\n";

        return response;
    }

private:
    int port_;
    int server_fd_ = -1;
    int epoll_fd_ = -1;
    std::atomic<bool> running_{false};

    std::thread io_thread_;
    std::thread business_thread_;

    ThreadSafeQueue<IncomingMessage> incoming_queue_;
    ThreadSafeQueue<OutgoingMessage> outgoing_queue_;

    std::mutex clients_mutex_;
    std::unordered_map<int, std::string> client_buffers_;
};

// ============================================================================
// TCP Plugin
// ============================================================================
class TcpServerPlugin
{
public:
    TcpServerPlugin(uint16_t t_port) : m_server(t_port) {}

    auto init() -> void
    {
        std::cout << "=== TCP Server with epoll (Two-Thread Architecture) ===\n\n";
    }
    auto run() -> void
    {
        if (!m_server.start()) {
            throw std::runtime_error("Failed to start server");
        }
    }

    auto stop() -> void
    {
        m_server.stop();
    }

private:
    TcpServer m_server;
};

// ============================================================================
// Main
// ============================================================================
int main()
{
    zephyr::Application app{"TcpServer"};
    app.init();
    app.run();
    app.stop();

    zephyr::plugin::PluginConcept auto tcpPlugin = TcpServerPlugin{PORT};

    tcpPlugin.init();
    tcpPlugin.run();

    std::cout << "\nServer running. Press Ctrl+C to stop...\n";

    // Wait for Ctrl+C
    std::signal(SIGINT, [](int) { std::cout << "\nReceived SIGINT, shutting down...\n"; });

    pause();

    tcpPlugin.stop();

    return 0;
}
