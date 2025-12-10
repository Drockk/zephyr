#include "zephyr/http/httpSerializer.hpp"

namespace zephyr::http
{
auto HttpSerializer::serialize(const HttpResponse &t_response)
    -> std::string
{
    std::string result{ "HTTP/1.1 " };

    result += std::to_string(t_response.status_code) + " ";
    result += t_response.status_text + "\r\n";

    auto headers = t_response.headers;
    if (!headers.contains("Content-Length") && !t_response.body.empty()) {
        headers["Content-Length"] = std::to_string(t_response.body.size());
    }

    for (const auto& [name, value] : headers) {
        result += name + ": " + value + "\r\n";
    }

    result += "\r\n" + t_response.body;

    return result;
}
}
