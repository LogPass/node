#pragma once

#include "thread.hpp"

namespace logpass {

class EventLoopThread : public Thread {
public:
    EventLoopThread(const std::string& threadName = "") :
        m_threadName(threadName), m_context(), m_guard(asio::make_work_guard(m_context))
    {};

    ~EventLoopThread()
    {
        ASSERT(m_stopped);
    }

    // starts event loop thread
    virtual void start() override
    {
        ASSERT(!m_stopped);
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_thread = std::thread([this] {
            SET_THREAD_NAME(m_threadName);
            // lock and unlock to sync m_thread
            m_mutex.lock();
            m_mutex.unlock();
            m_context.run();
        });
    }

    // finishes all tasks and stops event loop thread
    virtual void stop() override
    {
        ASSERT(!m_stopped);
        asio::post(m_context, [this] {
            m_guard.reset();
        });

        Thread::stop();
        m_stopped = true;
    }

    void post(std::function<void(void)>&& function) const
    {
        ASSERT(!m_stopped);
        asio::post(m_context, std::move(function));
    }

    bool isStopped() const
    {
        return m_stopped;
    }

protected:
    const std::string m_threadName;
    mutable asio::io_context m_context;

private:
    asio::executor_work_guard<asio::io_context::executor_type> m_guard;
    std::atomic<bool> m_stopped = false;
};

}
