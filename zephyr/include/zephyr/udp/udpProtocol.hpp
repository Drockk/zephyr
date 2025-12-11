#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <vector>

#include "zephyr/common/resultSender.hpp"

namespace zephyr::udp {
struct UdpProtocol {
    struct InputType {
        std::vector<uint8_t> data;
        std::string source_ip;
        uint16_t source_port;
        uint16_t dest_port;
        sockaddr_in client_addr;
    };
    using OutputType = std::optional<std::vector<uint8_t>>;
    using ResultSenderType = common::ResultSender<OutputType>;
    
    static constexpr int socket_type = SOCK_DGRAM;
    static constexpr const char* name = "UDP";
};
}
