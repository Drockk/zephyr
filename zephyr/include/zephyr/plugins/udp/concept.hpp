#pragma once

#include <concepts>
#include <span>
#include <utility>

namespace zephyr::plugins::udp
{
template <class C>
concept HasOnMessage = requires(C t_class) {
    { t_class.onMessage(std::declval<std::span<char>>()) } -> std::same_as<void>;
};

template <class C>
concept ControllerConcept = HasOnMessage<C>;
}  // namespace zephyr::plugins::udp
