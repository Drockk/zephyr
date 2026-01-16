#include <cstdint>
#include <stdexcept>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <plugins/tcpServer/details/tcpServer.hpp>
#include <sys/socket.h>

namespace
{
auto createSocket()
{
    auto serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        throw std::runtime_error("Cannot create socket");
    }

    return serverFd;
}

auto allowReuseAddress(int32_t t_fd) noexcept
{
    int32_t opt = 1;
    setsockopt(t_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

auto bindSocket(int32_t t_fd, std::pair<const char*, uint16_t> t_endpoint)
{
    const auto& [address, port] = t_endpoint;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(address);
    addr.sin_port = htons(port);

    if (bind(t_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Cannot bind socket");
    }
}

auto listenOnSocket(int32_t t_fd)
{
    constexpr auto BACKLOG = 5;
    if (listen(t_fd, BACKLOG) < 0) {
        close(t_fd);
        throw std::runtime_error("Cannot listen on socket");
    }
}

auto closeSocket(int32_t t_fd)
{
    close(t_fd);
}

auto acceptConnection(int32_t t_fd)
{
    return accept(t_fd, nullptr, 0);
}

auto receiveData(int32_t t_fd, std::span<std::byte> t_buffer)
{
    return recv(t_fd, t_buffer.data(), t_buffer.size(), 0);
}

auto sendData(int32_t t_fd, const std::span<const std::byte> t_buffer)
{
    send(t_fd, t_buffer.data(), t_buffer.size(), 0);
}
}  // namespace

namespace plugins::details
{
TcpServer::TcpServer(std::pair<const char*, uint16_t> t_endpoint) : m_endpoint(t_endpoint) {}

TcpServer::~TcpServer()
{
    stop();
}

auto TcpServer::init() -> void
{
    m_serverFd = createSocket();
    allowReuseAddress(m_serverFd);
    bindSocket(m_serverFd, m_endpoint);
    listenOnSocket(m_serverFd);
}

auto TcpServer::stop() -> void
{
    if (m_clientFd != -1) {
        closeSocket(m_clientFd);
        m_clientFd = -1;
    }

    if (m_serverFd != -1) {
        closeSocket(m_serverFd);
        m_serverFd = -1;
    }
}

auto TcpServer::accept() -> void
{
    m_clientFd = acceptConnection(m_serverFd);
}

auto TcpServer::receive(std::span<std::byte> t_buffer) -> size_t
{
    return receiveData(m_clientFd, t_buffer);
}

auto TcpServer::send(const std::span<const std::byte> t_buffer) -> void
{
    sendData(m_clientFd, t_buffer);
}

auto TcpServer::closeClient() -> void
{
    if (m_clientFd >= 0) {
        closeSocket(m_clientFd);
        m_clientFd = -1;
    }
}
}  // namespace plugins::details
