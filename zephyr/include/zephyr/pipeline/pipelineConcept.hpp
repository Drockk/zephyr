#pragma once

#include <concepts>
#include <memory>

#include "zephyr/context/context.hpp"

namespace zephyr::pipeline
{
template<typename T, typename Protocol>
concept PipelineConcept = requires(T pipeline, typename Protocol::InputType input, 
                                    std::shared_ptr<context::Context> ctx) {
    { pipeline(std::move(input), ctx) } -> std::convertible_to<typename Protocol::ResultSenderType>;
};

}
