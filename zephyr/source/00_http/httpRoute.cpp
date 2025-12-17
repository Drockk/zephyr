#include "zephyr/http/httpRoute.hpp"

namespace zephyr::http
{
auto HttpRoute::matches(const std::string& t_request_method, const std::string& t_request_path) const
    -> bool
{
    if (m_method != "*" && m_method != t_request_method) {
        return false;
    }

    return std::regex_match(t_request_path, m_path_regex);
}

auto HttpRoute::extract_params(const std::string& t_path) const
    -> std::map<std::string, std::string>
{
    std::map<std::string, std::string> params;

    std::smatch match;
    if (std::regex_match(t_path, match, m_path_regex)) {
        for (size_t i = 0; i < m_param_names.size() && i + 1 < match.size(); ++i) {
            params[m_param_names[i]] = match[i + 1];
        }
    }

    return params;
}

auto HttpRoute::invoke(HttpRequest t_request, const context::Context& t_context) const
    -> HttpSender
{
    t_request.path_params = extract_params(t_request.path);
    return m_handler(t_request, t_context);
}

auto HttpRoute::compile_pattern(const std::string& t_pattern)
    -> void
{
    std::string regex_pattern;
    size_t pos = 0;

    while (pos < t_pattern.size()) {
        if (t_pattern[pos] == ':') {
            size_t end = t_pattern.find('/', pos);

            if (end == std::string::npos) {
                end = t_pattern.size();
            }

            auto param_name = t_pattern.substr(pos + 1, end - pos - 1);
            m_param_names.push_back(param_name);
            regex_pattern += "([^/]+)";
            pos = end;
        } else if (t_pattern[pos] == '*') {
            regex_pattern += ".*";
            pos++;
        } else {
            if (std::string(".+?^$()[]{}|\\").find(t_pattern[pos]) != std::string::npos) {
                regex_pattern += '\\';
            }

            regex_pattern += t_pattern[pos];
            pos++;
        }
    }

    m_path_regex = std::regex(regex_pattern);
}
}
