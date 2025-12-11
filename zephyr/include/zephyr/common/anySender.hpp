#pragma once

#include <exec/any_sender_of.hpp>
#include <stdexec/execution.hpp>

namespace zephyr::common
{
template<class... Sigs>
using any_sender_of = exec::any_receiver_ref<stdexec::completion_signatures<Sigs...>>::template any_sender<>;
}
