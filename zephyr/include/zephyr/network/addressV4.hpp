#pragma once

#include "zephyr/network/details/protocolConcept.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <netinet/in.h>

namespace zephyr::network
{
class AddressV4
{
public:
    using BytesType = std::array<uint8_t, 4>;

    explicit AddressV4(std::string_view t_address);

    constexpr explicit AddressV4(const BytesType& t_bytes) noexcept : m_bytes(t_bytes) {}

    constexpr explicit AddressV4(uint32_t t_address) noexcept;

    constexpr explicit AddressV4(const sockaddr_in& t_address) noexcept;

    [[nodiscard]] constexpr auto toBytes() const noexcept -> BytesType;
    [[nodiscard]] constexpr auto toUint() const noexcept -> uint32_t;
    [[nodiscard]] constexpr auto toNetworkOrder() const noexcept -> uint32_t;

    [[nodiscard]] static constexpr auto any() noexcept -> AddressV4;
    [[nodiscard]] static constexpr auto loopback() noexcept -> AddressV4;
    [[nodiscard]] static constexpr auto broadcast() noexcept -> AddressV4;

    [[nodiscard]] constexpr bool isLoopback() const noexcept;
    [[nodiscard]] constexpr bool isMulticast() const noexcept;
    [[nodiscard]] constexpr bool isUnspecified() const noexcept;

    [[nodiscard]] static auto fromString(std::string_view t_address) noexcept -> details::ParseResult<AddressV4>;
    [[nodiscard]] auto toString() const -> std::string;

    friend constexpr bool operator==(const AddressV4&, const AddressV4&) = default;
    friend constexpr auto operator<=>(const AddressV4&, const AddressV4&) = default;

private:
    BytesType m_bytes{};
};
}  // namespace zephyr::network
