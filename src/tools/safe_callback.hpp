#pragma once

namespace logpass {

// calback which requires to always be called
template<typename T>
class SafeCallback {
public:
    SafeCallback(const std::function<void(std::shared_ptr<T>)>&& cb) : m_cb(std::move(cb))
    {
        ASSERT(m_cb);
    }

    ~SafeCallback()
    {
        ASSERT(m_called);
    }

    SafeCallback(const SafeCallback& sc) : m_cb(std::move(sc.m_cb))
    {
        ASSERT(!sc.m_called);
        sc.m_cb = nullptr;
        sc.m_called = true;
    }

    SafeCallback(SafeCallback&& sc) : m_cb(std::move(sc.m_cb))
    {
        ASSERT(!sc.m_called);
        sc.m_cb = nullptr;
        sc.m_called = true;
    }

    SafeCallback& operator = (SafeCallback&& sc)
    {
        ASSERT(!sc.m_called);
        m_cb = std::move(sc.m_cb);
        sc.m_cb = nullptr;
        sc.m_called = true;
        return *this;
    }

    void operator ()(T&& value) const
    {
        call(std::move(value));
    }

    void operator ()(const std::shared_ptr<T>& value) const
    {
        call(value);
    }

    void call(T&& value) const
    {
        call(std::make_shared<T>(std::move(value)));
    }

    void call(const std::shared_ptr<T>& value) const
    {
        ASSERT(!m_called);
        m_cb(value);
        m_called = true;
    }

private:
    mutable std::function<void(std::shared_ptr<T>)> m_cb;
    mutable bool m_called = false;
};

}
