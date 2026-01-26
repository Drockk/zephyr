#pragma once

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
        auto work = stdexec::just() | stdexec::then([t_connection]() {
                        // stdexec::start_detached(connectionLoop(t_scheduler, t_connection));
                    })
                    | stdexec::then([this]() { m_controller.onConnect(); });

        return stdexec::starts_on(t_scheduler, work);
    }

    Controller m_controller;
};
}  // namespace plugins
