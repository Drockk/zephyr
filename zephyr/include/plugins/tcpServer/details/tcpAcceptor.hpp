#pragma once

#include "zephyr/network/socket.hpp"

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <memory>

namespace plugins::details
{
class TcpAcceptor
{
public:
    TcpAcceptor(zephyr::network::Socket&& t_socket);

    auto init() -> void;

    auto acceptConnections() -> stdexec::sender auto
    {
        stdexec::scheduler auto scheduler = m_pool->get_scheduler();

        auto acceptWork = stdexec::just() | stdexec::then([this] { return m_socket.accept(); });
        return stdexec::starts_on(scheduler, acceptWork);
    }

    auto stop() -> void;

private:
    zephyr::network::Socket m_socket;
    std::unique_ptr<exec::static_thread_pool> m_pool{std::make_unique<exec::static_thread_pool>(1)};
};
}  // namespace plugins::details