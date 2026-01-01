#pragma once

#include <stdexec/execution.hpp>

#include <deque>
#include <exception>
#include <memory>
#include <mutex>

#include "stdexec/__detail/__senders_core.hpp"

namespace zephyr::execution
{
template <stdexec::scheduler BaseScheduler>
class StrandScheduler
{
public:
    explicit StrandScheduler(BaseScheduler t_base) : m_state(std::make_shared<StrandState>(std::move(t_base))) {}

    friend auto tag_invoke(stdexec::schedule_t /*unused*/, const StrandScheduler& t_self)
    {
        return ScheduleSender{t_self.m_state};
    }

    friend auto operator==(const StrandScheduler& t_a, const StrandScheduler& t_b) noexcept
    {
        return t_a.m_state == t_b.m_state;
    }

    friend auto operator!=(const StrandScheduler& t_a, const StrandScheduler& t_b) noexcept
    {
        return !(t_a == t_b);
    }

private:
    struct TaskBase
    {
        virtual ~TaskBase() = default;
        virtual void execute() = 0;
    };

    template <typename Receiver>
    struct TaskImpl : TaskBase
    {
        Receiver receiver;

        explicit TaskImpl(Receiver t_receiver) noexcept(std::is_nothrow_move_constructible_v<Receiver>)
            : receiver(std::move(t_receiver))
        {}

        auto execute() -> void override
        {
            try {
                stdexec::set_value(std::move(receiver));
            } catch (...) {
                stdexec::set_error(std::move(receiver), std::current_exception());
            }
        }
    };

    struct StrandState : std::enable_shared_from_this<StrandState>
    {
        BaseScheduler base;
        std::mutex mutex;
        std::deque<std::unique_ptr<TaskBase>> queue;
        bool running{false};

        explicit StrandState(BaseScheduler t_scheduler) : base(std::move(t_scheduler)) {}

        void executeNext()
        {
            std::unique_ptr<TaskBase> task;

            {
                std::lock_guard lock(mutex);
                if (queue.empty()) {
                    running = false;
                    return;
                }

                task = std::move(queue.front());
                queue.pop_front();
            }

            task->execute();

            auto self = this->shared_from_this();
            auto continuation
                = stdexec::schedule(base) | stdexec::then([self = std::move(self)]() { self->executeNext(); });

            stdexec::start_detached(std::move(continuation));
        }

        template <typename Receiver>
        void enqueueTask(Receiver t_receiver)
        {
            bool shouldStart = false;
            auto task = std::make_unique<TaskImpl<Receiver>>(std::move(t_receiver));

            {
                std::lock_guard lock(mutex);
                queue.push_back(std::move(task));

                if (!running) {
                    running = true;
                    shouldStart = true;
                }
            }

            if (shouldStart) {
                auto self = this->shared_from_this();
                auto starter
                    = stdexec::schedule(base) | stdexec::then([self = std::move(self)]() { self->executeNext(); });

                stdexec::start_detached(std::move(starter));
            }
        }
    };

    std::shared_ptr<StrandState> m_state;

    struct ScheduleSender
    {
        using sender_concept = stdexec::sender_t;
        
        std::shared_ptr<StrandState> state;

        template <typename Receiver>
        struct OperationState
        {
            std::shared_ptr<StrandState> state;
            Receiver receiver;

            friend void tag_invoke(stdexec::start_t /*unused*/, OperationState& t_operation) noexcept
            {
                t_operation.state->enqueueTask(std::move(t_operation.receiver));
            }
        };

        template <typename Receiver>
        friend auto tag_invoke(stdexec::connect_t /*unused*/, ScheduleSender&& t_self, Receiver t_receiver)
        {
            return OperationState<Receiver>{std::move(t_self.state), std::move(t_receiver)};
        }

        template <typename Env>
        friend auto tag_invoke(stdexec::get_completion_signatures_t /*unused*/, const ScheduleSender& /*unused*/, const Env&)
        {
            return stdexec::completion_signatures<stdexec::set_value_t()>{};
        }
    };
};
}  // namespace zephyr::execution
