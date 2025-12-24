#pragma once

#include "zephyr/network/endpoint.hpp"

namespace zephyr::network
{
class UdpSocket
{
public:
    constexpr explicit UdpSocket(UdpEndpoint t_endpoint) noexcept : m_endpoint(t_endpoint), m_socket(0) {}

    auto bind() -> void;

private:
    UdpEndpoint m_endpoint;
    int m_socket;
};
}  // namespace zephyr::network
