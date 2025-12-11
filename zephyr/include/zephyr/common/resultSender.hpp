#pragma once

#include "zephyr/common/anySender.hpp"

#include <stdexec/execution.hpp>

namespace zephyr::common
{
template<typename T>
using ResultSender = any_sender_of<
    stdexec::set_value_t(T),
    stdexec::set_error_t(std::exception_ptr),
    stdexec::set_stopped_t()
>;
}
