#pragma once

#include "zephyr/context/context.hpp"
#include "zephyr/http/httpRoute.hpp"

#include <memory>
#include <stdexec/execution.hpp>
#include <vector>

namespace zephyr::http
{
class HttpRouter
{
    using HttpSender = HttpRoute::HttpSender;

public:
    HttpRouter(): m_context(std::make_shared<context::Context>()) {}

    template<typename T>
    auto add_resource(const std::string& t_name, std::shared_ptr<T> t_resource)
        -> void
    {
        m_context->set(t_name, t_resource);
    }

    template<typename SyncHandler>
    auto add_route(std::string t_method, std::string t_path, SyncHandler t_handler)
    {
        auto async_handler
            = [h = std::move(t_handler)] (const HttpRequest& t_request, const context::Context& t_context) {
            return HttpSender{stdexec::just(h(t_request, t_context))};
        };

        m_routes.emplace_back(std::move(t_method), std::move(t_path), std::move(async_handler));
    }

    auto get(std::string t_path, auto t_handler)
        -> void
    {
        add_route("GET", std::move(t_path), std::move(t_handler));
    }

    auto post(std::string t_path, auto t_handler)
        -> void
    {
        add_route("POST", std::move(t_path), std::move(t_handler));
    }

    template<typename AsyncHandler>
    auto get_async(std::string t_path, AsyncHandler t_handler)
        -> void
    {
        m_routes.emplace_back(
            "GET",
            std::move(t_path),
            [h = std::move(t_handler)](const HttpRequest& t_request, const context::Context& t_context) {
                return HttpSender{h(t_request, t_context)};
            }
        );
    }

    auto route(HttpRequest t_request) const
        -> HttpSender;

private:
    std::vector<HttpRoute> m_routes;
    std::shared_ptr<context::Context> m_context;
};
}
