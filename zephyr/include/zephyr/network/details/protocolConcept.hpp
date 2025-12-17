#pragma once

#include <concepts>
#include <optional>

namespace zephyr::network::details
{
struct TcpTag
{};

struct UdpTag
{};

inline constexpr TcpTag TCP{};
inline constexpr UdpTag UDP{};

template <typename T>
concept Protocol = std::same_as<T, TcpTag> || std::same_as<T, UdpTag>;

template <typename T>
using ParseResult = std::optional<T>;
}  // namespace zephyr::network::details
