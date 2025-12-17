#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <stdexec/execution.hpp>
#include <queue>

namespace zephyr::execution
{
template<typename BaseScheduler>
struct StrandState: std::enable_shared_from_this<StrandState<BaseScheduler>>
{
    BaseScheduler m_base;
    std::mutex m_mutex;
    std::queue<std::function<void()>> m_tasks;
    bool m_active{ false };

    explicit StrandState(BaseScheduler t_base)
        : m_base(t_base) {}

    auto enqueue(std::function<void()> t_handler) -> void
    {
        auto start = false;

        {
            std::lock_guard lock(m_mutex);

            m_tasks.push(std::move(t_handler));

            if (!m_active) {
                m_active = true;
                start = true;
            }
        }

        if (start) {
            dispatch();
        }
    }

    auto dispatch() -> void
    {
        auto self = this->shared_from_this();

        stdexec::start_detached(
            stdexec::schedule(m_base)
                | stdexec::then([self] { self->run_one(); })
        );
    }

    auto run_one() -> void
    {
        std::function<void()> handler;

        {
            std::lock_guard lock(m_mutex);

            if (m_tasks.empty()) {
                m_active = false;
                return;
            }

            handler = std::move(m_tasks.front());
            m_tasks.pop();
        }

        try {
            handler();
        } catch(...) {

        }

        dispatch();
    }
};
}
