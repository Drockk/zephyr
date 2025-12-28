#pragma once

#include "zephyr/network/addressV4.hpp"
#include "zephyr/network/addressV6.hpp"
#include "zephyr/network/details/protocolConcept.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

#include <netinet/in.h>
#include <sys/socket.h>

#include "spdlog/fmt/bundled/base.h"

namespace zephyr::network
{
template <details::Protocol P>
class Endpoint
{
public:
    using ProtocolType = P;

    constexpr Endpoint(const AddressV4& t_address, uint16_t t_port) noexcept : m_address(t_address), m_port(t_port) {}
    constexpr Endpoint(const AddressV6& t_address, uint16_t t_port) noexcept
        : m_address(t_address),
          m_port(t_port),
          m_isV6(true)
    {}

    explicit Endpoint(const sockaddr* t_address, socklen_t t_length) : m_address(AddressV4::any())
    {
        if (t_address->sa_family == AF_INET && t_length >= sizeof(sockaddr_in)) {
            const auto* v4Address = reinterpret_cast<const sockaddr_in*>(t_address);
            m_address = AddressV4{*v4Address};
            m_port = std::byteswap(v4Address->sin_port);
            m_isV6 = false;
        } else if (t_address->sa_family == AF_INET6 && t_length >= sizeof(sockaddr_in6)) {
            const auto* v6Address = reinterpret_cast<const sockaddr_in6*>(t_address);
            m_address = AddressV6{*v6Address};
            m_port = std::byteswap(v6Address->sin6_port);
            m_isV6 = true;
        } else {
            m_address = AddressV4::any();
            m_port = 0;
            m_isV6 = false;
        }
    }

    [[nodiscard]] std::pair<sockaddr_storage, socklen_t> toSockaddr() const noexcept
    {
        sockaddr_storage storage{};

        if (m_isV6) {
            auto address = std::get<AddressV6>(m_address);
            auto* addr = reinterpret_cast<sockaddr_in6*>(&storage);
            addr->sin6_family = AF_INET6;
            addr->sin6_port = std::byteswap(m_port);
            addr->sin6_flowinfo = 0;
            addr->sin6_scope_id = address.scopeId();

            auto bytes = address.toBytes();
            std::copy(bytes.begin(), bytes.end(), std::begin(addr->sin6_addr.s6_addr));

            return {storage, sizeof(sockaddr_in6)};
        }

        auto address = std::get<AddressV4>(m_address);
        auto* addr = reinterpret_cast<sockaddr_in*>(&storage);
        addr->sin_family = AF_INET;
        addr->sin_port = std::byteswap(m_port);
        addr->sin_addr.s_addr = address.toNetworkOrder();

        return {storage, sizeof(sockaddr_in)};
    }

    [[nodiscard]] static constexpr Endpoint fromV4(const AddressV4& t_address, uint16_t t_port) noexcept
    {
        return Endpoint{t_address, t_port};
    }

    [[nodiscard]] static constexpr Endpoint fromV6(const AddressV6& t_address, uint16_t t_port) noexcept
    {
        return Endpoint{t_address, t_port};
    }

    [[nodiscard]] static details::ParseResult<Endpoint> fromString(std::string_view t_string) noexcept
    {
        if (t_string.starts_with('[')) {
            auto closeBracket = t_string.find(']');
            if (closeBracket == std::string_view::npos) {
                return std::nullopt;
            }

            auto addrStr = t_string.substr(1, closeBracket - 1);
            auto addr = AddressV6::fromString(addrStr);
            if (!addr) {
                return std::nullopt;
            }

            // Port
            if (closeBracket + 1 >= t_string.size() || t_string[closeBracket + 1] != ':') {
                return std::nullopt;
            }

            auto portStr = t_string.substr(closeBracket + 2);
            int portVal = 0;
            auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), portVal);

            if (ec != std::errc{} || portVal < 0 || portVal > 65535) {
                return std::nullopt;
            }

            return Endpoint{*addr, static_cast<uint16_t>(portVal)};
        }

        auto colonPos = t_string.rfind(':');
        if (colonPos == std::string_view::npos) {
            return std::nullopt;
        }

        auto addrStr = t_string.substr(0, colonPos);
        auto portStr = t_string.substr(colonPos + 1);

        auto addr = AddressV4::fromString(addrStr);
        if (!addr) {
            return std::nullopt;
        }

        int portVal = 0;
        auto [ptr, ec] = std::from_chars(portStr.data(), portStr.data() + portStr.size(), portVal);

        if (ec != std::errc{} || portVal < 0 || portVal > 65535) {
            return std::nullopt;
        }

        return Endpoint{*addr, static_cast<uint16_t>(portVal)};
    }

    [[nodiscard]] constexpr bool isV4() const noexcept
    {
        return !m_isV6;
    }
    [[nodiscard]] constexpr bool isV6() const noexcept
    {
        return m_isV6;
    }

    [[nodiscard]] constexpr auto port() const noexcept
    {
        return m_port;
    }
    constexpr auto port(uint16_t t_port) noexcept
    {
        m_port = t_port;
    }

    [[nodiscard]] constexpr auto addressV4() const noexcept
    {
        return std::get<AddressV4>(m_address);
    }

    [[nodiscard]] constexpr auto addressV6() const noexcept
    {
        return std::get<AddressV6>(m_address);
    }

    [[nodiscard]] auto toString() const
    {
        if (m_isV6) {
            return std::format("[{}]:{}", std::get<AddressV6>(m_address).toString(), m_port);
        }

        return std::format("{}:{}", std::get<AddressV4>(m_address).toString(), m_port);
    }

    [[nodiscard]] static constexpr auto isTcp() noexcept
    {
        return std::same_as<P, details::TcpTag>;
    }

    [[nodiscard]] static constexpr auto isUdp() noexcept
    {
        return std::same_as<P, details::UdpTag>;
    }

    [[nodiscard]] static constexpr auto socketType() noexcept
    {
        if constexpr (std::same_as<P, details::TcpTag>) {
            return SOCK_STREAM;
        } else {
            return SOCK_DGRAM;
        }
    }

    [[nodiscard]] static constexpr auto protocol() noexcept
    {
        if constexpr (std::same_as<P, details::TcpTag>) {
            return IPPROTO_TCP;
        } else {
            return IPPROTO_UDP;
        }
    }

    friend constexpr bool operator==(const Endpoint&, const Endpoint&) = default;

private:
    std::variant<AddressV4, AddressV6> m_address;

    uint16_t m_port{};
    bool m_isV6{};
};

using TcpEndpoint = Endpoint<details::TcpTag>;
using UdpEndpoint = Endpoint<details::UdpTag>;
}  // namespace zephyr::network

template <zephyr::network::details::Protocol P>
struct std::formatter<zephyr::network::Endpoint<P>> : std::formatter<std::string>
{
    auto format(const zephyr::network::Endpoint<P>& t_endpoint, std::format_context& t_ctx) const
    {
        return std::formatter<std::string>::format(t_endpoint.toString(), t_ctx);
    }
};

template <zephyr::network::details::Protocol P>
struct fmt::formatter<zephyr::network::Endpoint<P>>
{
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const zephyr::network::Endpoint<P>& t_endpoint, fmt::format_context& t_ctx) const
    {
        return fmt::format_to(t_ctx.out(), "{}", t_endpoint.toString());
    }
};