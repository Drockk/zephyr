#pragma once

#include <stdexec/execution.hpp>
#include <memory>
#include <type_traits>

#include "zephyr/context/context.hpp"

namespace zephyr::pipelines
{
template<typename Protocol, typename Handler>
class RawPipeline {
    Handler m_handler;
    
public:
    constexpr explicit RawPipeline(Handler t_handler) : m_handler(std::move(t_handler)) {}
    
    auto operator()(typename Protocol::InputType t_input, 
                    std::shared_ptr<context::Context>) const {
        return typename Protocol::ResultSenderType{stdexec::just(m_handler(std::move(t_input)))};
    }
};

template<typename Protocol, typename Handler>
RawPipeline(Handler) -> RawPipeline<Protocol, Handler>;

template<typename Protocol, typename Handler>
constexpr auto make_raw_pipeline(Handler&& t_handler) {
    return RawPipeline<Protocol, std::decay_t<Handler>>(std::forward<Handler>(t_handler));
}
}
