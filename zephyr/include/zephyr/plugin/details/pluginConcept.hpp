#pragma once

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <concepts>
#include <utility>

namespace zephyr::plugin
{
template <class C>
concept HasFuncInit = requires(C t_class) {
    { t_class.init() } -> std::same_as<void>;
};

template <class C>
concept HasFuncRun = requires(C t_class) {
    { t_class.run(std::declval<exec::static_thread_pool::scheduler>()) } -> std::same_as<void>;
};

template <class C>
concept HasFuncStop = requires(C t_class) {
    { t_class.stop() } -> std::same_as<void>;
};

template <class C>
concept PluginConcept = HasFuncInit<C> && HasFuncRun<C> && HasFuncStop<C>;
}  // namespace zephyr::plugin
