#pragma once

#include <stdexec/execution.hpp>
#include <memory>
#include <string>
#include <tuple>
#include <iostream>

#include "zephyr/context/context.hpp"
#include "zephyr/http/httpParser.hpp"
#include "zephyr/http/httpRouter.hpp"
#include "zephyr/http/httpSerializer.hpp"
#include "zephyr/tcp/tcpProtocol.hpp"

namespace zephyr::http
{
template<typename... Middlewares>
class HttpPipelineWithMiddleware
{
public:
    HttpPipelineWithMiddleware(const HttpRouter& t_router, Middlewares... t_midllewares)
        : m_router(t_router), m_middlewares(std::move(t_midllewares)...) {}

    auto operator()(std::string data, std::shared_ptr<context::Context> ctx)
        -> tcp::TcpProtocol::ResultSenderType
    {
        m_receive_buffer += data;

        if (!HttpParser::is_complete(m_receive_buffer)) {
            return {stdexec::just(
                tcp::TcpProtocol::OutputType{std::nullopt}
            )};
        }

        auto maybe_request = HttpParser::parse(m_receive_buffer);
        m_receive_buffer.clear();

        if (!maybe_request) {
            HttpResponse error_response{};
            error_response.status_code = 400;
            error_response.status_text = "Bad Request";
            error_response.body = "Failed to parse HTTP request";
            return {stdexec::just(
                tcp::TcpProtocol::OutputType{ HttpSerializer::serialize(error_response) }
            )};
        }

        std::cout << "[HTTP] " << maybe_request->method << " " << maybe_request->path << "\n";

        return apply_middlewares(std::move(*maybe_request), std::index_sequence_for<Middlewares...>{});
    }

private:
    template<std::size_t... Is>
    auto apply_middlewares(HttpRequest t_request, std::index_sequence<Is...>)
        -> tcp::TcpProtocol::ResultSenderType
    {
        auto sender = common::ResultSender<HttpRequest>{stdexec::just(std::move(t_request))};

        ((sender = common::ResultSender<HttpRequest>{
            std::move(sender) | stdexec::let_value(std::get<Is>(m_middlewares))
        }), ...);

        return tcp::TcpProtocol::ResultSenderType{
            std::move(sender)
                | stdexec::let_value([this](HttpRequest req) {
                    return m_router.route(req);
                })
                | stdexec::then([](HttpResponse resp) -> tcp::TcpProtocol::OutputType {
                    return HttpSerializer::serialize(resp);
                })
                | stdexec::upon_error([](std::exception_ptr e) -> tcp::TcpProtocol::OutputType {
                    try { std::rethrow_exception(e); }
                    catch (const std::exception& ex) {
                        std::cout << "[HTTP] Middleware error: " << ex.what() << "\n";
                        HttpResponse error_resp{};
                        error_resp.status_code = 401;
                        error_resp.status_text = "Unauthorized";
                        error_resp.body = ex.what();
                        return HttpSerializer::serialize(error_resp);
                    }
                })
        };
    }

    const HttpRouter& m_router;
    std::string m_receive_buffer;
    std::tuple<Middlewares...> m_middlewares;
};
}
