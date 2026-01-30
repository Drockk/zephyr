#pragma once

#include "zephyr/network/socket.hpp"

#include <exec/async_scope.hpp>
#include <exec/repeat_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "plugins/tcpServer/details/tcpAcceptor.hpp"
#include "plugins/tcpServer/details/tcpConnection.hpp"
#include "plugins/tcpServer/details/tcpServerControllerConcept.hpp"

namespace plugins
{
template <details::TcpServerControllerConcept Controller>
class TcpServer
{
public:
    TcpServer(const char* t_address, uint16_t t_port)
        : m_acceptor{{{t_address, t_port}}},
          m_connectionScope{std::make_unique<exec::async_scope>()}
    {}

    TcpServer(TcpServer&& t_other) noexcept
        : m_controller{std::move(t_other.m_controller)},
          m_acceptor{std::move(t_other.m_acceptor)},
          m_ioPool{std::move(t_other.m_ioPool)},
          m_connectionScope{std::move(t_other.m_connectionScope)},  // unique_ptr jest movable
          m_buffer{std::move(t_other.m_buffer)},
          m_shouldStop{t_other.m_shouldStop.load()}
    {}

    TcpServer& operator=(TcpServer&& t_other) noexcept
    {
        if (this != &t_other) {
            m_controller = std::move(t_other.m_controller);
            m_acceptor = std::move(t_other.m_acceptor);
            m_ioPool = std::move(t_other.m_ioPool);
            m_connectionScope = std::move(t_other.m_connectionScope);
            m_buffer = std::move(t_other.m_buffer);
            m_shouldStop.store(t_other.m_shouldStop.load());
        }
        return *this;
    }

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    auto init() -> void
    {
        m_acceptor.init();
    }

    auto run(stdexec::scheduler auto t_scheduler) -> void
    {
        auto acceptLoop = m_acceptor.acceptConnections()
                          | stdexec::let_value([this, t_scheduler](details::TcpConnection t_connection) {
                                m_connectionScope->spawn(connectionLoop(t_scheduler, std::move(t_connection))
                                                             | stdexec::upon_error([](std::exception_ptr) {}),
                                                         t_scheduler);
                                return stdexec::just(m_shouldStop.load());
                            })
                          | exec::repeat_until();

        stdexec::start_detached(acceptLoop);
    }

    auto stop() -> void
    {
        m_shouldStop.store(true);
        m_acceptor.stop();

        stdexec::sync_wait(m_connectionScope->on_empty());
    }

private:
    auto connectionLoop(stdexec::scheduler auto t_scheduler, details::TcpConnection t_connection)
    {
        return stdexec::transfer_just(t_scheduler) | stdexec::then([this, t_connection] {
                   auto bytesRead = t_connection.receive(m_buffer);
                   if (bytesRead == 0) {
                       throw std::runtime_error("Connection closed");
                   }
                   return bytesRead;
               })
               | stdexec::then([this](size_t t_readLength) {
                     return m_controller.onMessage(std::span<std::byte>(m_buffer.data(), t_readLength));
                 })
               | stdexec::then([this, t_connection](std::span<std::byte> t_response) {
                     if (!t_response.empty()) {
                         std::ignore = t_connection.send(t_response);
                     }
                     return m_shouldStop.load();
                 })
               | stdexec::upon_error([](std::exception_ptr) { return true; }) | exec::repeat_until();
    }

    Controller m_controller{};
    details::TcpAcceptor m_acceptor;
    std::unique_ptr<exec::static_thread_pool> m_ioPool{std::make_unique<exec::static_thread_pool>(1)};
    std::unique_ptr<exec::async_scope> m_connectionScope;  // ← unique_ptr zamiast bezpośrednio
    std::array<std::byte, 16 * 1024> m_buffer{};
    std::atomic<bool> m_shouldStop{false};
};
}  // namespace plugins