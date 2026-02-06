#include <cstddef>
#include <iostream>
#include <span>
#include <string>

#include <zephyr/zephyr.hpp>

class UdpServer
{
public:
    auto onMessage(std::span<std::byte> t_message) -> std::span<std::byte>
    {
        std::string text(reinterpret_cast<const char*>(t_message.data()), t_message.size());
        std::cout << text << "\n";

        return {};
    }
};

int main()
{
    return zephyr::runApplication(zephyr::Application{"UdpServer"});
}
