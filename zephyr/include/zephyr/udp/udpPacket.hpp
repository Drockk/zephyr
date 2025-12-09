#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zephyr::udp
{
struct UdpPacket {
    std::string source_ip;
    int source_port;
    int dest_port;
    std::vector<uint8_t> data;
};
}
