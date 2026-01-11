#pragma once

#include <stdexec/execution.hpp>

#include <concepts>

namespace zephyr::plugin
{
template <class C>
concept HasFuncInit = requires(C t_class) {
    { t_class.init() } -> std::same_as<void>;  // TODO: Check scheduler concept also
};

template <class C>
concept HasFuncRun = requires(C t_class) {
    { t_class.run() } -> std::same_as<void>;  // TODO: Check scheduler concept also
};

template <class C>
concept HasFuncStop = requires(C t_class) {
    { t_class.stop() } -> std::same_as<void>;  // TODO: Check scheduler concept also
};

template <class C>
concept PluginConcept = HasFuncInit<C> && HasFuncRun<C> && HasFuncStop<C>;
}  // namespace zephyr::plugin
