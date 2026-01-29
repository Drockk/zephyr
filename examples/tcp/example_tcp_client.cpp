#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <span>
#include <string>
#include <unistd.h>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <plugins/tcpClient/tcpClient.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <zephyr/zephyr.hpp>

class TcpClientController
{
public:
    [[nodiscard]] auto connectTo() const -> std::pair<const char*, uint16_t>
    {
        return {"127.0.0.1", 8080};
    }

    auto onConnect() -> std::span<std::byte>
    {
        static std::string msg = "Hello, Server!";
        return std::as_writable_bytes(std::span{msg});
    }

    auto onMessage(std::span<std::byte> t_message) -> std::span<std::byte>
    {
        std::string message(reinterpret_cast<const char*>(t_message.data()), t_message.size());
        std::cout << "Received: " << message << "\n";

        m_messageIndex++;
        if (m_messageIndex < m_messages.size()) {
            return std::as_writable_bytes(std::span{m_messages[m_messageIndex]});
        }

        return {};
    }

private:
    int m_messageIndex = -1;
    std::vector<std::string> m_messages
        = {"Hello, Server!", "This is a test message.", "How are you doing?", "Zephyr networking example", "Goodbye!"};
};

int main()
{
    return zephyr::runApplication(zephyr::Application{"TcpClient", plugins::TcpClient<TcpClientController>{}});
}
