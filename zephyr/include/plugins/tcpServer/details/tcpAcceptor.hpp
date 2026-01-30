// zephyr/include/plugins/tcpServer/details/tcpAcceptor.hpp
#pragma once

#include "zephyr/network/socket.hpp"

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <memory>

#include "plugins/tcpServer/details/tcpConnection.hpp"

namespace plugins::details
{
class TcpAcceptor
{
public:
    TcpAcceptor(zephyr::network::Socket&& t_socket);

    auto init() -> void;

    auto acceptConnections() -> stdexec::sender auto
    {
        return stdexec::transfer_just(m_pool->get_scheduler())
               | stdexec::then([this] { return TcpConnection(zephyr::network::Socket{m_socket.accept()}); });
    }

    auto stop() -> void;

private:
    zephyr::network::Socket m_socket;
    std::unique_ptr<exec::static_thread_pool> m_pool{std::make_unique<exec::static_thread_pool>(1)};
};
}  // namespace plugins::details