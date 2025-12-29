#pragma once

#include "zephyr/core/logger.hpp"
#include "zephyr/network/endpoint.hpp"

namespace zephyr::network
{
class UdpSocket
{
public:
    explicit UdpSocket(UdpEndpoint t_endpoint) noexcept;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& t_other) noexcept;
    UdpSocket& operator=(UdpSocket&& t_other) noexcept;
    ~UdpSocket() noexcept;

    auto bind(core::Logger::LoggerPtr& t_logger) -> void;
    auto close() noexcept -> void;

private:
    UdpEndpoint m_endpoint;
    int m_socket;
};
}  // namespace zephyr::network
