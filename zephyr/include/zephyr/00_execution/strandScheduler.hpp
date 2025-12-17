#pragma once

#include <memory>
#include <stdexec/execution.hpp>

#include "zephyr/execution/strandSender.hpp"
#include "zephyr/execution/strandState.hpp"

namespace zephyr::execution
{
template<typename BaseScheduler>
class StrandScheduler {
    std::shared_ptr<StrandState<BaseScheduler>> m_state;

public:
    explicit StrandScheduler(BaseScheduler t_base)
        : m_state(std::make_shared<StrandState<BaseScheduler>>(std::move(t_base))) {}
    
    friend auto tag_invoke(stdexec::schedule_t, const StrandScheduler& t_scheduler)
    {
        return StrandSender<BaseScheduler>{t_scheduler.m_state};
    }

    friend bool operator==(const StrandScheduler& a, const StrandScheduler& b) = default;
};
}