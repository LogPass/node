#pragma once

class TimeTester
{
public:
    TimeTester(const std::string& name) : m_name(name), m_start(chrono::steady_clock::now())
    {

    }

    ~TimeTester()
    {
        BOOST_TEST_MESSAGE(m_name << " execution time: " << (chrono::steady_clock::now() - m_start).count() / 1000000 << " ms.");
    }

private:
    std::string m_name;
    chrono::steady_clock::time_point m_start;
};
