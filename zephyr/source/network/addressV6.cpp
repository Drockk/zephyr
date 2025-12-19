#include "zephyr/network/addressV6.hpp"

#include "zephyr/network/details/protocolConcept.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace zephyr::network
{
AddressV6::AddressV6(const sockaddr_in6& t_address) noexcept : m_scopeId(t_address.sin6_scope_id)
{
    std::copy(std::begin(t_address.sin6_addr.s6_addr), std::end(t_address.sin6_addr.s6_addr), m_bytes.begin());
}

auto AddressV6::fromString(std::string_view t_address) noexcept -> details::ParseResult<AddressV6>
{
    BytesType bytes{};
    uint32_t scope = 0;

    auto scopePos = t_address.find('%');
    std::string_view addrStr = t_address;

    if (scopePos != std::string_view::npos) {
        auto scopeStr = t_address.substr(scopePos + 1);
        addrStr = t_address.substr(0, scopePos);

        int scopeVal = 0;
        auto [ptr, ec] = std::from_chars(scopeStr.data(), scopeStr.data() + scopeStr.size(), scopeVal);

        if (ec == std::errc{}) {
            scope = static_cast<uint32_t>(scopeVal);
        }
    }

    if (addrStr.size() > INET6_ADDRSTRLEN - 1) {
        return std::nullopt;
    }

    std::array<char, INET6_ADDRSTRLEN> buffer{};
    std::copy(addrStr.begin(), addrStr.end(), buffer.data());
    buffer.at(addrStr.size()) = '\0';

    in6_addr addr{};
    if (inet_pton(AF_INET6, buffer.data(), &addr) != 1) {
        return std::nullopt;
    }

    std::copy(std::begin(addr.s6_addr), std::end(addr.s6_addr), bytes.begin());

    return AddressV6{bytes, scope};
}

auto AddressV6::toString() const -> std::string
{
    std::array<char, INET6_ADDRSTRLEN> buffer{};
    in6_addr addr{};
    std::copy(m_bytes.begin(), m_bytes.end(), std::begin(addr.s6_addr));

    if (inet_ntop(AF_INET6, &addr, buffer.data(), buffer.size()) == nullptr) {
        return "::";
    }

    std::string result(buffer.data());
    if (m_scopeId != 0) {
        result += '%';
        result += std::to_string(m_scopeId);
    }

    return result;
}
}  // namespace zephyr::network
