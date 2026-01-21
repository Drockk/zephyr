#include <cstddef>
#include <iostream>
#include <span>
#include <string>

#include <plugins/tcpServer/tcpServer.hpp>
#include <zephyr/zephyr.hpp>

class TcpServerController
{
public:
    auto onMessage(std::span<std::byte> t_message) -> std::span<std::byte>
    {
        std::string text(reinterpret_cast<const char*>(t_message.data()), t_message.size());
        std::cout << "TcpController\n";
        std::cout << text << "\n";

        return t_message;
    }
};

int main()
{
    return zephyr::runApplication(
        zephyr::Application{"TcpServer", plugins::TcpServer<TcpServerController>{"127.0.0.1", 8080}});
}
