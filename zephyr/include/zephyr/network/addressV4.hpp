#pragma once

#include "zephyr/network/details/protocolConcept.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include <netinet/in.h>

namespace zephyr::network
{
class AddressV4
{
public:
    using BytesType = std::array<uint8_t, 4>;

    constexpr explicit AddressV4() = default;

    explicit AddressV4(std::string_view t_address);

    constexpr explicit AddressV4(const BytesType& t_bytes) noexcept : m_bytes(t_bytes) {}

    constexpr explicit AddressV4(uint32_t t_address) noexcept
    {
        m_bytes[0] = static_cast<uint8_t>((t_address >> 24) & 0xFF);
        m_bytes[1] = static_cast<uint8_t>((t_address >> 16) & 0xFF);
        m_bytes[2] = static_cast<uint8_t>((t_address >> 8) & 0xFF);
        m_bytes[3] = static_cast<uint8_t>(t_address & 0xFF);
    }

    constexpr explicit AddressV4(const sockaddr_in& t_address) noexcept
    {
        const auto ip = ntohl(t_address.sin_addr.s_addr);
        m_bytes[0] = static_cast<uint8_t>((ip >> 24) & 0xFF);
        m_bytes[1] = static_cast<uint8_t>((ip >> 16) & 0xFF);
        m_bytes[2] = static_cast<uint8_t>((ip >> 8) & 0xFF);
        m_bytes[3] = static_cast<uint8_t>(ip & 0xFF);
    }

    [[nodiscard]] constexpr auto toBytes() const noexcept -> BytesType
    {
        return m_bytes;
    }
    [[nodiscard]] constexpr auto toUint() const noexcept -> uint32_t
    {
        return (static_cast<uint32_t>(m_bytes[0]) << 24) | (static_cast<uint32_t>(m_bytes[1]) << 16)
               | (static_cast<uint32_t>(m_bytes[2]) << 8) | static_cast<uint32_t>(m_bytes[3]);
    }
    [[nodiscard]] constexpr auto toNetworkOrder() const noexcept -> uint32_t
    {
        const auto value = toUint();
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(value);
        } else {
            return value;
        }
    }

    [[nodiscard]] static constexpr auto any() noexcept -> AddressV4
    {
        return AddressV4{0};
    }
    [[nodiscard]] static constexpr auto loopback() noexcept -> AddressV4
    {
        return AddressV4{BytesType{127, 0, 0, 1}};
    }
    [[nodiscard]] static constexpr auto broadcast() noexcept -> AddressV4
    {
        return AddressV4{BytesType{255, 255, 255, 255}};
    }

    [[nodiscard]] constexpr auto isLoopback() const noexcept -> bool
    {
        return m_bytes[0] == 127;
    }
    [[nodiscard]] constexpr auto isMulticast() const noexcept -> bool
    {
        return (m_bytes[0] & 0xF0) == 0xE0;
    }
    [[nodiscard]] constexpr auto isUnspecified() const noexcept -> bool
    {
        return toUint() == 0;
    }

    [[nodiscard]] static auto fromString(std::string_view t_address) noexcept -> details::ParseResult<AddressV4>;
    [[nodiscard]] auto toString() const -> std::string;

    friend constexpr auto operator==(const AddressV4&, const AddressV4&) -> bool = default;
    friend constexpr auto operator<=>(const AddressV4&, const AddressV4&) = default;

private:
    BytesType m_bytes{};
};
}  // namespace zephyr::network

template <>
struct std::formatter<zephyr::network::AddressV4> : std::formatter<std::string>
{
    auto format(const zephyr::network::AddressV4& t_addr, std::format_context& t_ctx) const
    {
        return std::formatter<std::string>::format(t_addr.toString(), t_ctx);
    }
};
