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

constexpr AddressV4::AddressV4(const uint32_t t_address) noexcept
{
    m_bytes[0] = static_cast<uint8_t>((t_address >> 24) & 0xFF);
    m_bytes[1] = static_cast<uint8_t>((t_address >> 16) & 0xFF);
    m_bytes[2] = static_cast<uint8_t>((t_address >> 8) & 0xFF);
    m_bytes[3] = static_cast<uint8_t>(t_address & 0xFF);
}

constexpr AddressV4::AddressV4(const sockaddr_in& t_address) noexcept
{
    const auto ip = ntohl(t_address.sin_addr.s_addr);
    m_bytes[0] = static_cast<uint8_t>((ip >> 24) & 0xFF);
    m_bytes[1] = static_cast<uint8_t>((ip >> 16) & 0xFF);
    m_bytes[2] = static_cast<uint8_t>((ip >> 8) & 0xFF);
    m_bytes[3] = static_cast<uint8_t>(ip & 0xFF);
}

constexpr auto AddressV4::toBytes() const noexcept -> BytesType
{
    return m_bytes;
}

constexpr auto AddressV4::toUint() const noexcept -> uint32_t
{
    return (static_cast<uint32_t>(m_bytes[0]) << 24) | (static_cast<uint32_t>(m_bytes[1]) << 16)
           | (static_cast<uint32_t>(m_bytes[2]) << 8) | static_cast<uint32_t>(m_bytes[3]);
}

constexpr auto AddressV4::toNetworkOrder() const noexcept -> uint32_t
{
    return htonl(toUint());
}

constexpr auto AddressV4::any() noexcept -> AddressV4
{
    return AddressV4{0};
}

constexpr auto AddressV4::loopback() noexcept -> AddressV4
{
    return AddressV4{BytesType{127, 0, 0, 1}};
}

constexpr auto AddressV4::broadcast() noexcept -> AddressV4
{
    return AddressV4{BytesType{255, 255, 255, 255}};
}

constexpr auto AddressV4::isLoopback() const noexcept -> bool
{
    return m_bytes.at(0) == 127;
}

constexpr auto AddressV4::isMulticast() const noexcept -> bool
{
    return (m_bytes[0] & 0xF0) == 0xE0;
}

constexpr auto AddressV4::isUnspecified() const noexcept -> bool
{
    return toUint() == 0;
}

auto AddressV4::fromString(std::string_view t_address) noexcept -> details::ParseResult<AddressV4>
{
    if (t_address.size() > 15) {
        return std::nullopt;
    }

    std::array<char, 16> buffer{};
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
