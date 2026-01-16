#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace zephyr::details
{
class SignalHandler
{
public:
    static SignalHandler& instance()
    {
        static SignalHandler handler;
        return handler;
    }

    auto wait() -> void
    {
        std::unique_lock lock{m_mutex};
        m_cv.wait(lock, [this] { return m_signaled.load(); });
    }

    auto notify() -> void
    {
        m_signaled.store(true);
        m_cv.notify_all();
    }

    auto reset() -> void
    {
        m_signaled.store(false);
    }

    auto isSignaled() const -> bool
    {
        return m_signaled.load();
    }

private:
    SignalHandler() = default;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_signaled{false};
};

inline void signalCallback(int /*signal*/)
{
    SignalHandler::instance().notify();
}
}  // namespace zephyr::details
