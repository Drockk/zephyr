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
    
    static HttpResponse ok(std::string t_body_text)
    {
        HttpResponse r;
        r.body = std::move(t_body_text);
        return r;
    }
    
    static HttpResponse json(std::string t_json_text)
    {
        HttpResponse r;
        r.headers["Content-Type"] = "application/json";
        r.body = std::move(t_json_text);
        return r;
    }
    
    static HttpResponse not_found()
    {
        HttpResponse r;
        r.status_code = 404;
        r.status_text = "Not Found";
        r.body = "404 Not Found";
        return r;
    }
};
}
