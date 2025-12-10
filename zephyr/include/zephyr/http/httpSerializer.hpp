#pragma once

#include "zephyr/http/httpMessages.hpp"

#include <string>

namespace zephyr::http
{
class HttpSerializer
{
public:
    static auto serialize(const HttpResponse& t_response)
        -> std::string;
};
}
