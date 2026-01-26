#pragma once

#include "zephyr/network/socket.hpp"

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <cstdint>
#include <memory>
#include <utility>

#include "plugins/tcpServer/details/tcpConnection.hpp"

namespace plugins::details
{
class TcpConnector
{
public:
    auto connect(const std::pair<const char*, uint16_t> t_endpoint) -> stdexec::sender auto
    {
        stdexec::scheduler auto scheduler = m_pool->get_scheduler();

        auto work
            = stdexec::just(t_endpoint) | stdexec::then([](const std::pair<const char*, uint16_t> t_serverEndpoint) {
                  zephyr::network::Socket socket;
                  socket.create();
                  socket.connect(t_serverEndpoint);

                  return TcpConnection(std::move(socket));
              });

        return stdexec::starts_on(scheduler, work);
    }

private:
    std::unique_ptr<exec::static_thread_pool> m_pool{std::make_unique<exec::static_thread_pool>(1)};
};
}  // namespace plugins::details