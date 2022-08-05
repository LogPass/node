#pragma once

namespace logpass {

class Thread : public std::enable_shared_from_this<Thread> {
public:
    Thread() = default;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    virtual ~Thread()
    {
        ASSERT(m_stopped);
        ASSERT(!m_thread.joinable());
    }

    virtual void start() = 0;

    // stop all tasks, destructor should be called immediately after this function
    virtual void stop()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
        ASSERT(!m_stopped);
        ASSERT(weak_from_this().use_count() <= 1);
        m_stopped = true;
    };

protected:
    std::thread m_thread;
    mutable std::shared_mutex m_mutex;

private:
    std::atomic<bool> m_stopped = false;
};

}
