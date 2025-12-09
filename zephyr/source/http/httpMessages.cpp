#include "zephyr/http/httpMessages.hpp"

namespace zephyr::http
{
auto HttpResponse::ok(std::string t_body_text) ->HttpResponse
{
    HttpResponse r;
    r.body = std::move(t_body_text);
    return r;
}

auto HttpResponse::json(std::string t_json_text) -> HttpResponse
{
    HttpResponse r;
    r.headers["Content-Type"] = "application/json";
    r.body = std::move(t_json_text);
    return r;
}

auto HttpResponse::not_found() -> HttpResponse
{
    HttpResponse r;
    r.status_code = 404;
    r.status_text = "Not Found";
    r.body = "404 Not Found";
    return r;
}
}
