module;

#include <concepts>
#include <expected>

export module zephyr.plugin;

namespace zephyr
{
export enum class Result
{
    ok,
    error
};

export template<class C>
concept HasFuncInit = requires(C c)
{
    { c.init() } -> std::same_as<Result>;
};

export template<class C>
concept HasFuncStart = requires(C c)
{
    { c.start() } -> std::same_as<Result>;
};

export template<class C>
concept PluginConcept = HasFuncInit<C> && HasFuncStart<C>;
}
