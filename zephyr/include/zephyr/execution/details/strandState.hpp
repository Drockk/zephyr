#pragma once

#include <functional>
#include <mutex>
#include <stdexec/execution.hpp>
#include <queue>

namespace zephyr::execution::details
{
template<stdexec::scheduler BaseScheduler>
class StrandState: public std::enable_shared_from_this<StrandState<BaseScheduler>>
{
public:
    using Operation = std::function<void()>;

    template<typename Scheduler>
    explicit StrandState(Scheduler t_scheduler)
        : m_base_scheduler(t_scheduler) {}

    auto schedule_operation(Operation t_operation)
    {
        auto should_run_now = false;

        {
            std::lock_guard<std::mutex> lock (m_mutex);
            m_pending_operations.push(std::move(t_operation));

            if (!m_is_running) {
                m_is_running = true;
                should_run_now = true;
            }
        }

        if (should_run_now) {
            run_next_operation();
        }
    }

private:
    auto run_next_operation()
    {
        Operation operation_to_run;

        {
            std::lock_guard<std::mutex> lock (m_mutex);
            if (m_pending_operations.empty()) {
                m_is_running = false;
                return;
            }

            operation_to_run = std::move(m_pending_operations.front());
            m_pending_operations.pop();
        }

        auto work = stdexec::schedule(m_base_scheduler)
            | stdexec::then([operation = std::move(operation_to_run)](){
                operation();
            })
            | stdexec::then([self = this->shared_from_this()](){
                self->run_next_or_stop();
            })
            | stdexec::upon_error([self = this->shared_from_this()](std::exception_ptr) {
                self->run_next_or_stop();
            });

        stdexec::start_detached(std::move(work));
    }

    auto run_bext_or_stop()
    {
        bool has_more = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            has_more = !m_pending_operations.empty();
            if (!has_more) {
                m_is_running = false;
            }
        }

        if (has_more) {
            run_next_operation();
        }
    }

    std::queue<Operation> m_pending_operations{};
    bool m_is_running{ false };
    std::mutex m_mutex;

    BaseScheduler m_base_scheduler;
};
}
