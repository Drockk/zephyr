#pragma once

#include <string>
#include <optional>
#include "zephyr/http/httpMessages.hpp"

namespace zephyr::http
{
class HttpParser
{
public:
    static auto parse(const std::string& t_raw)
        -> std::optional<HttpRequest>;
    
    static auto is_complete(const std::string& data)
        -> bool;
};
}
