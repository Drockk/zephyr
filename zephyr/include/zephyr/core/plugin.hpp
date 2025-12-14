#pragma once

#include <concepts>

namespace zephyr::core
{
enum class Result
{
    OK,
    ERROR
};

template <class C>
concept HasFuncInit = requires(C t_class) {
    { t_class.init() } -> std::same_as<Result>;
};

template <class C>
concept HasFuncStart = requires(C t_class) {
    { t_class.start() } -> std::same_as<Result>;
};

template <class C>
concept PluginConcept = HasFuncInit<C> && HasFuncStart<C>;
}  // namespace zephyr::core
