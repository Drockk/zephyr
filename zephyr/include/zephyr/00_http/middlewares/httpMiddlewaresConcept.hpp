#pragma once

#include <concepts>

#include "zephyr/common/resultSender.hpp"
#include "zephyr/http/httpMessages.hpp"

namespace zephyr::http
{
template<typename F>
concept HttpMiddlewareConcept = requires(F f, HttpRequest req) {
    { f(std::move(req)) } -> std::convertible_to<common::ResultSender<http::HttpRequest>>;
};
}
