#pragma once

#include <concepts>

namespace zephyr::core
{
template <class C>
concept HasFuncInit = requires(C t_class) {
    { t_class.init() } -> std::same_as<void>;
};

// template <class C>
// concept HasFuncStart = requires(C t_class) {
//     t_class.start(std::declval<int>());  // Accept any scheduler, verify at instantiation
// };

template <class C>
concept HasFuncStop = requires(C t_class) {
    { t_class.stop() } -> std::same_as<void>;
};

template <class C>
concept PluginConcept = HasFuncInit<C> /*&& HasFuncStart<C>*/ && HasFuncStop<C>;
}  // namespace zephyr::core
