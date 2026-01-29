#pragma once

#include <exec/repeat_until.hpp>
#include <stdexec/execution.hpp>

#include "plugins/tcpClient/details/tcpClientControllerConcept.hpp"
#include "plugins/tcpClient/details/tcpConnector.hpp"

namespace plugins
{
template <details::TcpClientControllerConcept Controller>
class TcpClient
{
public:
    auto init() -> void {}
    auto run(stdexec::scheduler auto t_scheduler) -> void
    {
        details::TcpConnector connector;
        auto connectionWork = connector.connect(m_controller.connectTo())
                              | stdexec::then([this, t_scheduler](details::TcpConnection t_connection) {
                                    stdexec::start_detached(connectionLoop(t_scheduler, t_connection));
                                });

        stdexec::start_detached(connectionWork);
    }
    auto stop() -> void {}

private:
    auto connectionLoop(stdexec::scheduler auto t_scheduler, details::TcpConnection t_connection)
    {
        auto work = stdexec::just() | stdexec::then([this]() { return m_controller.onConnect(); })
                    | stdexec::then([this, t_connection](std::span<std::byte> t_response) {
                          std::ignore = t_connection.send(t_response);
                      })
                    | stdexec::then([this, t_connection]() {
                          auto messageLoop
                              = stdexec::just()
                                | stdexec::then([this, t_connection]() { return t_connection.receive(m_buffer); })
                                | stdexec::then([this](size_t t_readLength) -> std::pair<std::span<std::byte>, bool> {
                                      if (t_readLength == 0) {
                                          return {{}, true};
                                      }

                                      const auto response
                                          = m_controller.onMessage(std::span<std::byte>(m_buffer.data(), t_readLength));

                                      if (response.empty()) {
                                          return {response, true};
                                      }

                                      return {response, false};
                                  })
                                | stdexec::then([this, t_connection](std::pair<std::span<std::byte>, bool> t_result) {
                                      const auto [response, shouldStop] = t_result;

                                      if (!response.empty()) {
                                          std::ignore = t_connection.send(response);
                                      }

                                      return shouldStop;
                                  })
                                | exec::repeat_until();

                          return messageLoop;
                      })
                    | stdexec::let_value([](auto messageLoop) { return messageLoop; });

        return stdexec::starts_on(t_scheduler, work);
    }

    Controller m_controller;
    std::array<std::byte, 16 * 1024> m_buffer{};
};
}  // namespace plugins
