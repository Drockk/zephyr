#pragma once

#include <memory>
#include <stdexec/execution.hpp>

#include "zephyr/execution/strandOp.hpp"

namespace zephyr::execution
{
template<typename BaseScheduler>
struct StrandSender {
    std::shared_ptr<StrandState<BaseScheduler>> strand;
    
    using completion_signatures = stdexec::completion_signatures<
        stdexec::set_value_t(),
        stdexec::set_error_t(std::exception_ptr),
        stdexec::set_stopped_t()
    >;
    
    template<class Receiver>
    auto connect(Receiver t_receiver) const {
        return StrandOp<BaseScheduler, Receiver>{strand, std::move(t_receiver)};
    }
};
}

template<typename Base>
inline constexpr bool stdexec::enable_sender<zephyr::execution::StrandSender<Base>> = true;
