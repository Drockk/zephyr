/**
 * @file example_udp_client.cpp
 * @brief Standalone UDP client example with epoll and two-thread architecture
 *
 * Features:
 * - Asynchronous I/O using epoll
 * - IO thread: handles epoll events and socket operations
 * - Business logic thread: generates messages and processes responses
 * - Thread-safe message queue for communication between threads
 * - Sends messages to server and receives modified responses
 *
 * Compile: g++ -std=c++20 -pthread -o udp_client example_udp_client.cpp
 */

#include "zephyr/plugin/details/pluginConcept.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
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

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 8081;
constexpr int MAX_EVENTS = 16;
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

    bool wait_and_pop_timeout(T& item, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || stopped_; })) {
            return false;
        }
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
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
        return stopped_;
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
struct OutgoingMessage
{
    std::string data;
};

struct IncomingMessage
{
    sockaddr_in server_addr;
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

int create_udp_client_socket(const char* server_ip, int server_port, sockaddr_in& server_addr)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        std::cerr << "Failed to create socket\n";
        return -1;
    }

    server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
        close(sock_fd);
        return -1;
    }

    // Connect UDP socket to server (allows using send/recv instead of sendto/recvfrom)
    if (connect(sock_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        std::cerr << "Connect failed: " << strerror(errno) << "\n";
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
// UDP Client with epoll
// ============================================================================
class UdpClient
{
public:
    UdpClient(const char* server_ip, int server_port) : server_ip_(server_ip), server_port_(server_port) {}

    ~UdpClient()
    {
        stop();
    }

    bool start()
    {
        socket_fd_ = create_udp_client_socket(server_ip_, server_port_, server_addr_);
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
        business_thread_ = std::thread(&UdpClient::business_logic_loop, this);

        // Start I/O thread
        io_thread_ = std::thread(&UdpClient::io_loop, this);

        std::cout << "Connected to " << server_ip_ << ":" << server_port_ << " (UDP)\n";
        std::cout << "IO thread and Business Logic thread running\n";

        return true;
    }

    void stop()
    {
        if (!running_) {
            return;
        }

        running_ = false;
        outgoing_queue_.stop();
        incoming_queue_.stop();

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

        std::cout << "Client stopped\n";
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

    bool is_running() const
    {
        return running_;
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
                running_ = false;
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
            ssize_t bytes_read = recv(socket_fd_, buffer, BUFFER_SIZE, 0);

            if (bytes_read > 0) {
                std::string data(buffer, bytes_read);

                // Send to business logic thread
                incoming_queue_.push({server_addr_, data});
                std::cout << "[IO Thread] Received " << bytes_read << " bytes\n";

            } else if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // No more data
                }
                std::cerr << "[IO Thread] recv error: " << strerror(errno) << "\n";
                running_ = false;
                break;
            }
        }
    }

    void process_outgoing_messages()
    {
        OutgoingMessage msg;
        while (outgoing_queue_.try_pop(msg)) {
            ssize_t sent = send(socket_fd_, msg.data.data(), msg.data.size(), 0);

            if (sent == -1) {
                std::cerr << "[IO Thread] send error: " << strerror(errno) << "\n";
            } else {
                std::cout << "[IO Thread] Sent " << sent << " bytes\n";
            }
        }
    }

    // Business logic thread: generates messages and processes responses
    void business_logic_loop()
    {
        std::cout << "[Business Thread] Started\n";

        // Example messages to send
        std::vector<std::string> messages
            = {"Hello, UDP Server!", "This is a UDP test message.", "UDP is connectionless!",
               "Zephyr UDP networking example", "Goodbye from UDP client!"};

        int message_index = 0;
        int responses_received = 0;

        while (running_) {
            // Send a message every 2 seconds
            if (message_index < static_cast<int>(messages.size())) {
                std::string msg = messages[message_index];
                std::cout << "[Business Thread] Sending: \"" << msg << "\"\n";

                outgoing_queue_.push({msg});
                message_index++;
            }

            // Wait for response with timeout
            IncomingMessage response;
            if (incoming_queue_.wait_and_pop_timeout(response, std::chrono::seconds(2))) {
                process_response(response.data);
                responses_received++;
            }

            // If we've sent all messages and received all responses, we can stop
            if (message_index >= static_cast<int>(messages.size())
                && responses_received >= static_cast<int>(messages.size())) {
                std::cout << "[Business Thread] All messages sent and responses received\n";
                running_ = false;
            }

            // Timeout - try to receive any remaining responses
            if (message_index >= static_cast<int>(messages.size())
                && responses_received < static_cast<int>(messages.size())) {
                // Wait a bit more for remaining responses
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        std::cout << "[Business Thread] Stopped\n";
    }

    void process_response(const std::string& data)
    {
        std::cout << "[Business Thread] Received response: \"" << data << "\"\n";

        // Example business logic: analyze the response
        if (data.find("ECHO") != std::string::npos) {
            std::cout << "[Business Thread] Server echoed our message (uppercase)\n";
        }
    }

private:
    const char* server_ip_;
    int server_port_;
    int socket_fd_ = -1;
    int epoll_fd_ = -1;
    sockaddr_in server_addr_{};
    std::atomic<bool> running_{false};

    std::thread io_thread_;
    std::thread business_thread_;

    ThreadSafeQueue<OutgoingMessage> outgoing_queue_;
    ThreadSafeQueue<IncomingMessage> incoming_queue_;
};

class UdpClientPlugin
{
public:
    UdpClientPlugin(const char* t_serverIp, int t_serverPort) : m_client(t_serverIp, t_serverPort) {}

    auto init() -> void
    {
        std::cout << "=== UDP Client with epoll (Two-Thread Architecture) ===\n\n";
    }

    auto run() -> void
    {
        if (!m_client.start()) {
            throw std::runtime_error("Failed to start client");
        }
    }

    auto stop() -> void
    {
        m_client.wait();
    }

private:
    UdpClient m_client;
};

// ============================================================================
// Main
// ============================================================================
int main()
{
    zephyr::plugin::PluginConcept auto client = UdpClientPlugin{SERVER_IP, SERVER_PORT};
    client.init();
    client.run();
    client.stop();

    return 0;
}
