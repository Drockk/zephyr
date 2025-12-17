#pragma once

#include <stdexec/execution.hpp>
#include <iostream>

#include "zephyr/common/resultSender.hpp"
#include "zephyr/http/httpMessages.hpp"

namespace zephyr::http
{
inline auto logging_middleware() {
    return [](HttpRequest req) -> common::ResultSender<HttpRequest> {
        std::cout << "[Middleware:Log] " << req.method << " " << req.path << "\n";
        return common::ResultSender<HttpRequest>{stdexec::just(std::move(req))};
    };
}
}
