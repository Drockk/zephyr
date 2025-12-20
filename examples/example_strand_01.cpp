#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <iostream>

#include <zephyr/execution/strandScheduler.hpp>

namespace ex = stdexec;

int main()
{
    exec::static_thread_pool pool(4);
    zephyr::execution::StrandScheduler strand(pool.get_scheduler());

    auto work = ex::schedule(strand)
                | ex::then([]() { std::cout << "Task 1 on thread: " << std::this_thread::get_id() << '\n'; })
                | ex::then([]() { std::cout << "Task 2 on thread: " << std::this_thread::get_id() << '\n'; });

    ex::start_detached(std::move(work));

    for (int i = 0; i < 10; ++i) {
        auto task = ex::schedule(strand) | ex::then([i]() {
                        std::cout << "Iteration " << i << " on thread: " << std::this_thread::get_id() << '\n';
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    });

        ex::start_detached(std::move(task));
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
}