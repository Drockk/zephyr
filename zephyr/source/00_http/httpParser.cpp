#include "zephyr/http/httpParser.hpp"

namespace zephyr::http
{
auto HttpParser::parse(const std::string& t_raw)
        -> std::optional<HttpRequest>
{
    HttpRequest req;

    auto first_line_end = t_raw.find("\r\n");
    if (first_line_end == std::string::npos) {
        return std::nullopt;
    }

    auto request_line = t_raw.substr(0, first_line_end);
    auto method_end = request_line.find(' ');
    if (method_end == std::string::npos) {
        return std::nullopt;
    }

    req.method = request_line.substr(0, method_end);
    auto path_start = method_end + 1;
    auto path_end = request_line.find(' ', path_start);
    if (path_end == std::string::npos) {
        return std::nullopt;
    }

    req.path = request_line.substr(path_start, path_end - path_start);
    req.version = request_line.substr(path_end + 1);

    auto pos = first_line_end + 2;
    while (true) {
        auto line_end = t_raw.find("\r\n", pos);
        if (line_end == std::string::npos) {
            break;
        }

        auto line = t_raw.substr(pos, line_end - pos);
        if (line.empty()) {
            pos = line_end + 2;
            break;
        }

        auto colon = line.find(':');
        if (colon != std::string::npos) {
            auto name = line.substr(0, colon);
            auto value = line.substr(colon + 2);
            req.headers[name] = value;
        }
        pos = line_end + 2;
    }

    if (pos < t_raw.size()) {
        req.body = t_raw.substr(pos);
    }

    return req;
}

auto HttpParser::is_complete(const std::string& t_data) -> bool
{
    return t_data.find("\r\n\r\n") != std::string::npos;
}
}
