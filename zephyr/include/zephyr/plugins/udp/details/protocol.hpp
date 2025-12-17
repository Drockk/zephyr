#pragma once

// #include "zephyr/common/resultSender.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <netinet/in.h>

namespace zephyr::plugins::udp
{
struct UdpProtocol
{
    struct InputType
    {
        std::string sourceIp{};
        uint16_t sourcePort;
        uint16_t destPort;
        sockaddr_in clientAddr;
        std::vector<uint8_t> data{};
    };

    using OutputType = std::optional<std::vector<uint8_t>>;
    // using ResultSenderType = common::ResultSender<OutputType>;

    static constexpr auto SOCKET_TYPE = SOCK_DGRAM;
    static constexpr auto* NAME = "UDP";
};
}  // namespace zephyr::plugins::udp