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
        auto connectionWork
            = connector.connect(m_controller.connectTo()) | stdexec::then([this]() { m_controller.onConnect(); });
        stdexec::start_detached(connectionWork);
    }
    auto stop() -> void {}

private:
    Controller m_controller;
};
}  // namespace plugins
