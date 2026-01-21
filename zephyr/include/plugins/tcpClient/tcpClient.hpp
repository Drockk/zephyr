#pragma once

#include <stdexec/execution.hpp>

#include "plugins/tcpClient/details/tcpClientControllerConcept.hpp"

namespace plugins
{
template <details::TcpClientControllerConcept Controller>
class TcpClient
{
public:
    auto init() -> void {}
    auto run(stdexec::scheduler auto t_scheduler) -> void {}
    auto stop() -> void {}
};
}  // namespace plugins
