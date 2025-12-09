#pragma once

#include <memory>
#include <stdexec/execution.hpp>
#include "zephyr/execution/strandState.hpp"

namespace zephyr::execution
{
template<typename BaseScheduler, class Receiver>
struct StrandOp
{
    std::shared_ptr<StrandState<BaseScheduler>> m_strand;
    Receiver m_receiver;

    auto start() noexcept -> void
    {
        auto self = m_strand;
        m_strand->enqueue([self, receiver = std::move(m_receiver)] mutable {
            try {
                stdexec::set_value(std::move(receiver));
            } catch (...) {
                stdexec::set_error(std::move(receiver), std::current_exception());
            }
        });
    }
};
}
