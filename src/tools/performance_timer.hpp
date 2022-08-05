#pragma once

namespace logpass {

class PerformanceTimer {
public:
    PerformanceTimer(const std::string& name, Logger* logger = nullptr) :
        m_name(name), m_logger(logger), m_start(chrono::steady_clock::now())
    {

    }

    ~PerformanceTimer()
    {
        size_t executionTime = (chrono::steady_clock::now() - m_start).count() / 1000000;
        if (m_logger) {
            BOOST_LOG_SEV(*m_logger, boost::log::trivial::info) << m_name <<
                " execution time: " << executionTime << " ms.";
        } else {
            LOG(info) << m_name << " execution time: " << executionTime << " ms.";
        }
    }

private:
    std::string m_name;
    Logger* m_logger = nullptr;
    chrono::steady_clock::time_point m_start;
};

}
