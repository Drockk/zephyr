#pragma once

#include <stdexec/execution.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

#include <plugins/tcpServer/details/tcpServerControllerConcept.hpp>

namespace plugins
{
template <details::TcpServerControllerConcept Controller>
class TcpServer
{
public:
    TcpServer(const char* t_address, uint16_t t_port) : m_address(t_address), m_port(t_port) {}

    auto init() -> void {}

    auto run() -> stdexec::sender auto
    {
        return begin() | preMessage() | onMessage() | postMessage();
    }

    auto stop() -> void {}

private:
    auto begin()
    {
        return stdexec::just();
    }

    auto preMessage()
    {
        return stdexec::then([] { std::cout << "Pre message\n"; });
    }

    auto onMessage()
    {
        return stdexec::then([this] -> std::span<std::byte> { return m_controller.onMessage(m_test); });
    }

    auto postMessage()
    {
        return stdexec::then([]([[maybe_unused]] std::span<std::byte> t_response) { std::cout << "Post message\n"; });
    }

    Controller m_controller{};

    const char* m_address;
    uint16_t m_port;

    // TEMP
    std::vector<std::byte> m_test;
};
}  // namespace plugins
