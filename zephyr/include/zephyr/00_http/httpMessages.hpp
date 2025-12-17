#pragma once

#include <map>
#include <string>

namespace zephyr::http
{
struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> path_params;
    std::string body;
};

struct HttpResponse
{
    int status_code = 200;
    std::string status_text = "OK";
    std::map<std::string, std::string> headers;
    std::string body;

    static auto ok(std::string t_body_text) -> HttpResponse;

    static auto json(std::string t_json_text) -> HttpResponse;

    static auto not_found() -> HttpResponse;
};
}
