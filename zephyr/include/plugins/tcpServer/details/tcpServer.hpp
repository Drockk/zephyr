#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace plugins::details
{
class TcpServer
{
public:
    TcpServer(std::pair<const char*, uint16_t> t_endpoint);
    ~TcpServer();

    auto init() -> void;
    auto stop() -> void;

    auto accept() -> void;
    auto receive(std::span<std::byte> t_buffer) -> size_t;
    auto send(std::span<const std::byte> t_buffer) -> void;
    auto closeClient() -> void;

private:
    std::pair<const char*, uint16_t> m_endpoint;
    int32_t m_serverFd{-1};
    int32_t m_clientFd{-1};
};
}  // namespace plugins::details