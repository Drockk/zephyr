#pragma once

#include <memory>

#include "zephyr/context/context.hpp"
#include "zephyr/http/httpRouter.hpp"
#include "zephyr/tcp/tcpProtocol.hpp"

namespace zephyr::http
{
class HttpPipeline
{
public:
    explicit HttpPipeline(const HttpRouter& t_router);

    auto operator()(std::string t_data, std::shared_ptr<context::Context>)
        -> tcp::TcpProtocol::ResultSenderType;

private:
    const HttpRouter& m_router;
    std::string m_receive_buffer;
};
}
