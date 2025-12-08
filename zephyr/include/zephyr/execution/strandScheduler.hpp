#pragma once

#include <memory>
#include <stdexec/execution.hpp>

#include "zephyr/execution/details/strandState.hpp"

namespace zephyr::execution
{
template<stdexec::scheduler BaseScheduler>
class StrandScheduler
{
public:
    explicit StrandScheduler(BaseScheduler base_sched) {
        m_state = std::make_shared<details::StrandState<BaseScheduler>>(std::move(base_sched));
    }

    StrandScheduler(const StrandScheduler&) = default;
    StrandScheduler& operator=(const StrandScheduler&) = default;

    auto schedule() const {
        return StrandSender<BaseScheduler>{m_state};
    }

private:
    template<stdexec::scheduler Sched>
    struct StrandSender {
        std::shared_ptr<details::StrandState<Sched>> strand_state;
        
        using completion_signatures = stdexec::completion_signatures<
            stdexec::set_value_t(),
            stdexec::set_error_t(std::exception_ptr),
            stdexec::set_stopped_t()
        >;
        
        template<typename Receiver>
        struct Operation {
            std::shared_ptr<details::StrandState<Sched>> strand_state;
            Receiver receiver;
            
            auto start() noexcept {
                try {
                    strand_state->schedule_operation([rcv = std::move(receiver)]() mutable {
                        stdexec::set_value(std::move(rcv));
                    });
                } catch (...) {
                    stdexec::set_error(std::move(receiver), std::current_exception());
                }
            }
        };

        auto connect(auto rcv) {
            return Operation{strand_state, std::move(rcv)};
        }
    };

    std::shared_ptr<details::StrandState<BaseScheduler>> m_state;
};

// template<stdexec::scheduler BaseScheduler>
auto make_strand_scheduler(stdexec::scheduler auto t_base_scheduler)
{
    return StrandScheduler{ std::move(t_base_scheduler) };
}
}
