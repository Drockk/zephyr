#pragma once

#include <concepts>
#include <cstddef>
#include <span>

namespace plugins::details
{
template <class C>
concept HasOnMessage = requires(C t_class, std::span<std::byte> t_data) {
    { t_class.onMessage(t_data) } -> std::same_as<std::span<std::byte>>;
};

template <class C>
concept TcpClientControllerConcept = HasOnMessage<C>;
}  // namespace plugins::details