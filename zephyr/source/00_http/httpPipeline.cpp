#include "zephyr/http/httpPipeline.hpp"

#include <stdexec/execution.hpp>
#include <iostream>

#include "zephyr/http/httpMessages.hpp"
#include "zephyr/http/httpParser.hpp"
#include "zephyr/http/httpSerializer.hpp"

namespace zephyr::http
{
HttpPipeline::HttpPipeline(const HttpRouter& t_router)
        : m_router(t_router) {}

auto HttpPipeline::operator()(std::string t_data, std::shared_ptr<context::Context>)
    -> tcp::TcpProtocol::ResultSenderType
{
    m_receive_buffer += t_data;

    if (!HttpParser::is_complete(m_receive_buffer)) {
        return {
            stdexec::just(tcp::TcpProtocol::OutputType(std::nullopt))
        };
    }

    auto maybe_request = HttpParser::parse(m_receive_buffer);
    m_receive_buffer.clear();

    if (!maybe_request) {
        HttpResponse error_response{};
        error_response.status_code = 400;
        error_response.status_text = "Bad Request";
        error_response.body = "Failed to parse HTTP request";

        return {
            stdexec::just(tcp::TcpProtocol::OutputType{
                HttpSerializer::serialize(error_response)
            })
        };
    }

    std::cout << "[HTTP] " << maybe_request->method << " " << maybe_request->path << "\n";

    return {
        m_router.route(*maybe_request)
        | stdexec::then([](HttpResponse t_response)
            -> tcp::TcpProtocol::OutputType {
            return HttpSerializer::serialize(t_response);
        })
    };
}
}
