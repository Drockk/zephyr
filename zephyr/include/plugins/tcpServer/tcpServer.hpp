#pragma once

#include "zephyr/network/socket.hpp"

#include <exec/repeat_effect_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>

#include "plugins/tcpServer/details/tcpAcceptor.hpp"
#include "plugins/tcpServer/details/tcpServer.hpp"
#include "plugins/tcpServer/details/tcpServerControllerConcept.hpp"

namespace plugins
{
template <details::TcpServerControllerConcept Controller>
class TcpServer
{
public:
    TcpServer(const char* t_address, uint16_t t_port) : m_acceptor{{{t_address, t_port}}} {}

    TcpServer(TcpServer&& t_other) noexcept
        : m_controller{std::move(t_other.m_controller)},
          m_acceptor{std::move(t_other.m_acceptor)},
          m_ioPool{std::move(t_other.m_ioPool)},
          m_buffer{std::move(t_other.m_buffer)},
          m_shouldStop{t_other.m_shouldStop.load()}
    {}

    TcpServer& operator=(TcpServer&& t_other) noexcept
    {
        if (this != &t_other) {
            m_controller = std::move(t_other.m_controller);
            m_acceptor = std::move(t_other.m_acceptor);
            m_ioPool = std::move(t_other.m_ioPool);
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
        auto serverLoop = m_acceptor.acceptConnections();

        // auto serverLoop
        //     = stdexec::just() | stdexec::let_value([this, t_scheduler, ioScheduler] {
        //           auto acceptWork
        //               = stdexec::starts_on(ioScheduler, stdexec::just() | stdexec::then([this] { m_server.accept();
        //               }));

        //           auto connectionLoop
        //               = stdexec::just() | stdexec::let_value([this, t_scheduler, ioScheduler] {
        //                     return stdexec::starts_on(ioScheduler, stdexec::just() | stdexec::then([this] {
        //                                                                return m_server.receive(m_buffer);
        //                                                            }))
        //                            | stdexec::continues_on(t_scheduler)
        //                            | stdexec::then(
        //                                [this](size_t t_readLength) -> std::pair<std::span<std::byte>, bool> {
        //                                    if (t_readLength == 0) {
        //                                        return {{}, true};
        //                                    }
        //                                    auto response = m_controller.onMessage(
        //                                        std::span<std::byte>(m_buffer.data(), t_readLength));
        //                                    return {response, false};
        //                                })
        //                            | stdexec::continues_on(ioScheduler)
        //                            | stdexec::then([this](std::pair<std::span<std::byte>, bool> result) {
        //                                  auto [response, shouldStop] = result;
        //                                  if (!response.empty()) {
        //                                      m_server.send(response);
        //                                  }
        //                                  return shouldStop || m_shouldStop.load();
        //                              });
        //                 })
        //                 | exec::repeat_effect_until();

        //           return std::move(acceptWork)
        //                  | stdexec::let_value([connectionLoop = std::move(connectionLoop)]() mutable {
        //                        return std::move(connectionLoop);
        //                    })
        //                  | stdexec::then([this] {
        //                        m_server.closeClient();
        //                        return m_shouldStop.load();
        //                    });
        //       })
        //       | exec::repeat_effect_until();

        stdexec::start_detached(serverLoop);
    }

    auto stop() -> void
    {
        m_shouldStop.store(true);
        m_acceptor.stop();
    }

private:
    Controller m_controller{};
    details::TcpAcceptor m_acceptor;
    std::unique_ptr<exec::static_thread_pool> m_ioPool{std::make_unique<exec::static_thread_pool>(1)};
    std::array<std::byte, 16 * 1024> m_buffer{};
    std::atomic<bool> m_shouldStop{false};
};
}  // namespace plugins
