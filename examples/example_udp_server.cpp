/**
 * @file example_udp_server.cpp
 * @brief Standalone UDP server example with epoll and two-thread architecture
 *
 * Features:
 * - Asynchronous I/O using epoll
 * - IO thread: handles epoll events and socket operations
 * - Business logic thread: processes messages and generates responses
 * - Thread-safe message queue for communication between threads
 * - Echo server that modifies received messages before sending back
 *
 * Compile: g++ -std=c++20 -pthread -o udp_server example_udp_server.cpp
 */

#include "zephyr/plugin/details/pluginConcept.hpp"

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
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>

constexpr int PORT = 8081;
constexpr int MAX_EVENTS = 64;
constexpr int BUFFER_SIZE = 65536;

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
    sockaddr_in client_addr;
    std::string data;
};

struct OutgoingMessage
{
    sockaddr_in client_addr;
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

int create_udp_socket(int port)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        std::cerr << "Failed to create socket\n";
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Failed to set SO_REUSEADDR\n";
        close(sock_fd);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "Failed to bind to port " << port << ": " << strerror(errno) << "\n";
        close(sock_fd);
        return -1;
    }

    if (!set_non_blocking(sock_fd)) {
        std::cerr << "Failed to set non-blocking\n";
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

// ============================================================================
// UDP Server with epoll
// ============================================================================
class UdpServer
{
public:
    UdpServer(int port) : port_(port) {}

    ~UdpServer()
    {
        stop();
    }

    bool start()
    {
        socket_fd_ = create_udp_socket(port_);
        if (socket_fd_ == -1) {
            return false;
        }

        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1) {
            std::cerr << "Failed to create epoll\n";
            close(socket_fd_);
            return false;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = socket_fd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_fd_, &ev) == -1) {
            std::cerr << "Failed to add socket to epoll\n";
            close(epoll_fd_);
            close(socket_fd_);
            return false;
        }

        running_ = true;

        // Start business logic thread
        business_thread_ = std::thread(&UdpServer::business_logic_loop, this);

        // Start I/O thread
        io_thread_ = std::thread(&UdpServer::io_loop, this);

        std::cout << "UDP Server started on port " << port_ << "\n";
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

        if (epoll_fd_ != -1) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
        if (socket_fd_ != -1) {
            close(socket_fd_);
            socket_fd_ = -1;
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
        char buffer[BUFFER_SIZE];

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
                if (events[i].events & EPOLLIN) {
                    handle_incoming_data(buffer);
                }
            }
        }

        std::cout << "[IO Thread] Stopped\n";
    }

    void handle_incoming_data(char* buffer)
    {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            ssize_t bytes_read
                = recvfrom(socket_fd_, buffer, BUFFER_SIZE, 0, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

            if (bytes_read > 0) {
                std::string data(buffer, bytes_read);

                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

                std::cout << "[IO Thread] Received " << bytes_read << " bytes from " << ip_str << ":"
                          << ntohs(client_addr.sin_port) << "\n";

                // Send to business logic thread
                incoming_queue_.push({client_addr, data});

            } else if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // No more data
                }
                std::cerr << "[IO Thread] recvfrom error: " << strerror(errno) << "\n";
                break;
            }
        }
    }

    void process_outgoing_messages()
    {
        OutgoingMessage msg;
        while (outgoing_queue_.try_pop(msg)) {
            ssize_t sent = sendto(socket_fd_, msg.data.data(), msg.data.size(), 0,
                                  reinterpret_cast<sockaddr*>(&msg.client_addr), sizeof(msg.client_addr));

            if (sent == -1) {
                std::cerr << "[IO Thread] sendto error: " << strerror(errno) << "\n";
            } else {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &msg.client_addr.sin_addr, ip_str, sizeof(ip_str));

                std::cout << "[IO Thread] Sent " << sent << " bytes to " << ip_str << ":"
                          << ntohs(msg.client_addr.sin_port) << "\n";
            }
        }
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
            std::string response = process_message(msg.client_addr, msg.data);

            // Send response back through IO thread
            if (!response.empty()) {
                outgoing_queue_.push({msg.client_addr, response});
            }
        }

        std::cout << "[Business Thread] Stopped\n";
    }

    std::string process_message(const sockaddr_in& client_addr, const std::string& data)
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

        std::cout << "[Business Thread] Processing message from " << ip_str << ":" << ntohs(client_addr.sin_port)
                  << ": \"" << data << "\"\n";

        // Business logic: transform the message
        // Example: convert to uppercase and add prefix
        std::string response = "[UDP ECHO] ";
        for (char c : data) {
            response += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        std::cout << "[Business Thread] Generated response: \"" << response << "\"\n";

        return response;
    }

private:
    int port_;
    int socket_fd_ = -1;
    int epoll_fd_ = -1;
    std::atomic<bool> running_{false};

    std::thread io_thread_;
    std::thread business_thread_;

    ThreadSafeQueue<IncomingMessage> incoming_queue_;
    ThreadSafeQueue<OutgoingMessage> outgoing_queue_;
};

class UdpServerPlugin
{
public:
    UdpServerPlugin(uint16_t t_port) : m_server(t_port) {}

    auto init() -> void
    {
        std::cout << "=== UDP Server with epoll (Two-Thread Architecture) ===\n\n";
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
    UdpServer m_server;
};

// ============================================================================
// Main
// ============================================================================
int main()
{
    auto plugin = UdpServerPlugin{PORT};

    plugin.init();
    plugin.run();

    std::cout << "\nServer running. Press Ctrl+C to stop...\n";

    // Wait for Ctrl+C
    std::signal(SIGINT, [](int) { std::cout << "\nReceived SIGINT, shutting down...\n"; });

    pause();

    plugin.stop();

    return 0;
}
