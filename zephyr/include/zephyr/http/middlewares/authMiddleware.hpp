#pragma once

#include <stdexec/execution.hpp>
#include <string>

#include "zephyr/common/resultSender.hpp"
#include "zephyr/http/httpMessages.hpp"

namespace zephyr::http
{
inline auto auth_middleware(std::string token) {
    return [token = std::move(token)](HttpRequest req) -> common::ResultSender<HttpRequest> {
        auto it = req.headers.find("Authorization");
        if (it == req.headers.end() || it->second != "Bearer " + token) {
            return common::ResultSender<HttpRequest>{
                stdexec::just(std::move(req)) | stdexec::then([](auto) -> HttpRequest {
                    throw std::runtime_error("Unauthorized");
                })
            };
        }
        return common::ResultSender<HttpRequest>{stdexec::just(std::move(req))};
    };
}
}
