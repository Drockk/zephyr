#pragma once

#include <sys/socket.h>
#include <optional>
#include <string>

#include "zephyr/common/resultSender.hpp"

namespace zephyr::tcp {
struct TcpProtocol
{
    using InputType = std::string;
    using OutputType = std::optional<std::string>;
    using ResultSenderType = common::ResultSender<OutputType>;

    static constexpr auto socket_type = SOCK_STREAM;
    static constexpr auto name = "TCP";
};
}
