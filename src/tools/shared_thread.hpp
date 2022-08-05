#pragma once

#include "thread.hpp"

namespace logpass {

template<class T>
class SharedThread : std::is_constructible<T> {
    static_assert(std::is_base_of<Thread, T>::value, "SharedThread template class must derive from Thread class");

    template <class V, class = decltype(V())>
    static std::true_type has_default_constructor(V*);

    template <class V>
    static std::false_type has_default_constructor(...);

    static constexpr bool has_default_constructor_v = decltype(has_default_constructor<T>(nullptr))::value;

public:
    SharedThread()
    {
        if constexpr (has_default_constructor_v) {
            m_thread = std::shared_ptr<T>(new T());
            m_thread->start();
        }
    }

    template<class... Types>
    SharedThread(Types&&... args) : m_thread(new T(std::forward<Types>(args)...))
    {
        m_thread->start();
    }

    ~SharedThread()
    {
        if (m_thread)
            m_thread->stop();
        m_thread = nullptr;
    }

    SharedThread(const SharedThread&) = delete;
    SharedThread& operator=(const SharedThread&) = delete;

    SharedThread(SharedThread&& s)
    {
        static_assert(!has_default_constructor_v, "can't move SharedThread if template class has default constructor");
        m_thread = std::move(s.m_thread);
        s.m_thread.reset();
    }

    SharedThread& operator=(SharedThread&& s)
    {
        static_assert(!has_default_constructor_v, "can't move SharedThread if template class has default constructor");
        ASSERT(!m_thread && s.m_thread);
        if (m_thread) {
            m_thread->stop();
        }
        m_thread = std::move(s.m_thread);
        s.m_thread.reset();
        return *this;
    }

    void reset()
    {
        if (m_thread) {
            m_thread->stop();
        }
        m_thread = nullptr;
    }

    operator bool() const
    {
        return !!m_thread;
    }

    operator const std::shared_ptr<T>() const
    {
        ASSERT(!!m_thread);
        return m_thread;
    }

    operator const std::shared_ptr<const T>() const
    {
        ASSERT(!!m_thread);
        return m_thread;
    }

    operator const std::weak_ptr<T>() const
    {
        ASSERT(!!m_thread);
        return m_thread;
    }

    operator const std::weak_ptr<const T>() const
    {
        ASSERT(!!m_thread);
        return m_thread;
    }

    T& operator*() const
    {
        ASSERT(!!m_thread);
        return *m_thread;
    }

    T* operator->() const
    {
        ASSERT(!!m_thread);
        return m_thread.get();
    }

private:
    std::shared_ptr<T> m_thread;
};

}
