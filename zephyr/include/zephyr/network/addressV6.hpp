#pragma once

#include "zephyr/network/details/protocolConcept.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <string_view>

#include <netinet/in.h>

namespace zephyr::network
{
class AddressV6
{
public:
    using BytesType = std::array<uint8_t, 16>;

    constexpr explicit AddressV6() noexcept = default;
    constexpr explicit AddressV6(const BytesType& t_bytes, uint32_t t_scope = 0) noexcept
        : m_bytes(t_bytes),
          m_scopeId(t_scope)
    {}
    explicit AddressV6(const sockaddr_in6& t_address) noexcept;

    [[nodiscard]] constexpr auto toBytes() const noexcept -> BytesType
    {
        return m_bytes;
    }

    [[nodiscard]] constexpr auto scopeId() const noexcept -> uint32_t
    {
        return m_scopeId;
    }
    constexpr auto scopeId(uint32_t t_id) noexcept -> void
    {
        m_scopeId = t_id;
    }

    [[nodiscard]] static constexpr auto any() noexcept -> AddressV6
    {
        return AddressV6{};
    }
    [[nodiscard]] static constexpr auto loopback() noexcept -> AddressV6
    {
        BytesType bytes{};
        bytes[15] = 1;
        return AddressV6{bytes};
    }

    [[nodiscard]] constexpr auto isLoopback() const noexcept -> bool
    {
        return std::ranges::all_of(m_bytes | std::views::take(15), [](auto t_byte) { return t_byte == 0; })
               && m_bytes[15] == 1;
    }
    [[nodiscard]] constexpr auto isMulticast() const noexcept -> bool
    {
        return m_bytes[0] == 0xFF;
    }
    [[nodiscard]] constexpr auto isLinkLocal() const noexcept -> bool
    {
        return m_bytes[0] == 0xFE && (m_bytes[1] & 0xC0) == 0x80;
    }
    [[nodiscard]] constexpr auto isUnspecified() const noexcept -> bool
    {
        return std::ranges::all_of(m_bytes, [](auto t_byte) { return t_byte == 0; });
    }

    [[nodiscard]] static auto fromString(std::string_view t_address) noexcept -> details::ParseResult<AddressV6>;
    [[nodiscard]] auto toString() const -> std::string;

    friend constexpr bool operator==(const AddressV6&, const AddressV6&) = default;
    friend constexpr auto operator<=>(const AddressV6&, const AddressV6&) = default;

private:
    BytesType m_bytes{};
    uint32_t m_scopeId{0};
};
}  // namespace zephyr::network

template <>
struct std::formatter<zephyr::network::AddressV6> : std::formatter<std::string>
{
    auto format(const zephyr::network::AddressV6& t_addr, std::format_context& t_ctx) const
    {
        return std::formatter<std::string>::format(t_addr.toString(), t_ctx);
    }
};
