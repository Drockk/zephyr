#include "zephyr/network/addressV4.hpp"

#include "zephyr/network/details/protocolConcept.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace zephyr::network
{
AddressV4::AddressV4(std::string_view t_address)
{
    const auto parsed = fromString(t_address);
    if (!parsed) {
        throw std::invalid_argument{"Invalid IPv4 address"};
    }

    m_bytes = parsed->toBytes();
}

auto AddressV4::fromString(std::string_view t_address) noexcept -> details::ParseResult<AddressV4>
{
    if (t_address.size() > INET_ADDRSTRLEN - 1) {
        return std::nullopt;
    }

    std::array<char, INET_ADDRSTRLEN> buffer{};
    std::copy(t_address.begin(), t_address.end(), buffer.data());
    buffer.at(t_address.size()) = '\0';

    in_addr address{};
    if (inet_pton(AF_INET, buffer.data(), &address) != 1) {
        return std::nullopt;
    }

    const auto ip = ntohl(address.s_addr);
    BytesType bytes{static_cast<uint8_t>((ip >> 24) & 0xFF), static_cast<uint8_t>((ip >> 16) & 0xFF),
                    static_cast<uint8_t>((ip >> 8) & 0xFF), static_cast<uint8_t>(ip & 0xFF)};

    return AddressV4{bytes};
}

auto AddressV4::toString() const -> std::string
{
    std::array<char, INET_ADDRSTRLEN> buffer{};
    in_addr address{};
    address.s_addr = htonl(toUint());

    if (inet_ntop(AF_INET, &address, buffer.data(), buffer.size()) == nullptr) {
        return "0.0.0.0";
    }

    return {buffer.data()};
}
}  // namespace zephyr::network
