#pragma once

#include "zephyr/core/logger.hpp"
#include "zephyr/network/endpoint.hpp"

namespace zephyr::network
{
class UdpSocket
{
public:
    explicit UdpSocket(core::Logger::LoggerPtr& t_logger, UdpEndpoint t_endpoint) noexcept;
    ~UdpSocket() noexcept;

    auto bind() -> void;
    auto close() noexcept -> void;

private:
    core::Logger::LoggerPtr m_logger;
    UdpEndpoint m_endpoint;
    int m_socket;
};
}  // namespace zephyr::network
