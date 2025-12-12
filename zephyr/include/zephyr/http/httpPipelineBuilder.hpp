#pragma once

#include <tuple>

#include "zephyr/http/httpPipeline.hpp"
#include "zephyr/http/httpPipelineWithMiddleware.hpp"
#include "zephyr/http/httpRouter.hpp"

namespace zephyr::http
{
template<typename... Middlewares>
class HttpPipelineBuilder
{
public:
    HttpPipelineBuilder() = default;

    HttpPipelineBuilder(const HttpRouter* t_router, std::tuple<Middlewares...> t_middlewares)
        : m_router(t_router), m_middlewares(std::move(t_middlewares)) {}

    auto with_router(const HttpRouter& t_router)
    {
        return HttpPipelineBuilder<Middlewares...>(&t_router, m_middlewares);
    }

    template<typename Middleware>
    auto with_middleware(Middleware&& t_middleware)
    {
        auto new_middlewares = std::tuple_cat(
            m_middlewares,
            std::make_tuple(std::forward<Middleware>(t_middleware))
        );

        return HttpPipelineBuilder<Middlewares..., std::decay_t<Middleware>>(
            m_router, std::move(new_middlewares)
        );
    }

    auto build() const
    {
        if (!m_router) {
            throw std::runtime_error("HttpPipelineBuilder: router not set");
        }
        
        const auto& router = *m_router;
        auto middleware = m_middlewares;
        
        if constexpr (sizeof...(Middlewares) == 0) {
            return [&router]() {
                return HttpPipeline(router);
            };
        } else {
            return [&router, middleware]() {
                return std::apply([&router](auto&&... args) {
                    return HttpPipelineWithMiddleware(router, std::forward<decltype(args)>(args)...);
                }, middleware);
            };
        }
    }


private:
    const HttpRouter* m_router{ nullptr };
    std::tuple<Middlewares...> m_middlewares;
};
}
