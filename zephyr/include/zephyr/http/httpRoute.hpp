#pragma once

#include <exception>
#include <functional>
#include <regex>
#include <stdexec/execution.hpp>
#include <string>
#include <vector>

#include "zephyr/context/context.hpp"
#include "zephyr/http/httpMessages.hpp"
#include "zephyr/utils/anySender.hpp"

namespace zephyr::http
{
class HttpRoute
{
public:
    using HttpSender = utils::any_sender_of<
        stdexec::set_value_t(HttpResponse),
        stdexec::set_error_t(std::exception_ptr),
        stdexec::set_stopped_t()
    >;

    using Handler = std::function<HttpSender(const HttpRequest&, const context::Context)>;

    template<typename H>
    HttpRoute(std::string t_method, std::string t_pattern, H t_handler)
        : m_method(std::move(t_method)), m_handler(std::move(t_handler))
    {
        compile_pattern(t_pattern);
    }

    auto matches(const std::string& t_request_method, const std::string& t_request_path) const
        -> bool;

    auto extract_params(const std::string& t_path) const
        -> std::map<std::string, std::string>;

    auto invoke(HttpRequest t_request, const context::Context& t_context) const
        -> HttpSender;

private:
    auto compile_pattern(const std::string& t_pattern)
        -> void;

    std::string m_method;
    std::regex m_path_regex;
    std::vector<std::string> m_param_names;
    Handler m_handler;
};
}

