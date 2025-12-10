#include "zephyr/http/httpRouter.hpp"

namespace zephyr::http
{
auto HttpRouter::route(HttpRequest t_request) const
    -> HttpSender
{
    for (const auto& r : m_routes) {
        if (r.matches(t_request.method, t_request.path)) {
            return r.invoke(std::move(t_request), *m_context);
        }
    }

    return HttpSender{stdexec::just(HttpResponse::not_found())};
}
}
