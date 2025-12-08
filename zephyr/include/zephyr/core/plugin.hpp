#pragma once

#include <concepts>

namespace zephyr::core
{
enum class Result
{
    ok,
    error
};

template<class C>
concept HasFuncInit = requires(C c)
{
    { c.init() } -> std::same_as<Result>;
};

template<class C>
concept HasFuncStart = requires(C c)
{
    { c.start() } -> std::same_as<Result>;
};

template<class C>
concept PluginConcept = HasFuncInit<C> && HasFuncStart<C>;
}
