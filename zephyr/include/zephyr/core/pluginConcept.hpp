#pragma once

#include <concepts>

namespace zephyr::core
{
template <class C, class Scheduler>
concept HasFuncInit = requires(C t_class, Scheduler t_scheduler) {
    { t_class.init(t_scheduler) } -> std::same_as<void>;
};

// template <class C>
// concept HasFuncStart = requires(C t_class) {
//     t_class.start(std::declval<int>());  // Accept any scheduler, verify at instantiation
// };

template <class C>
concept HasFuncStop = requires(C t_class) {
    { t_class.stop() } -> std::same_as<void>;
};

// Generic plugin concept that works with any scheduler type
template <class C>
concept PluginConcept = requires(C t_class) {
    { t_class.stop() } -> std::same_as<void>;
} && requires {
    // Has init that accepts something - will be validated at instantiation
    typename C;
};

}  // namespace zephyr::core
