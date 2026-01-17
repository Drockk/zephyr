#include <csignal>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <span>
#include <string>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <plugins/tcpServer/tcpServer.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <zephyr/core/application.hpp>

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

private:
};

int main()
{
    zephyr::Application app{"TcpServer", plugins::TcpServer<TcpServerController>{"127.0.0.1", 8080}};
    app.init();
    app.run();
    app.stop();

    return 0;
}
