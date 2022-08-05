#pragma once

namespace logpass {

#define LOG(lvl) BOOST_LOG_TRIVIAL(lvl)
#define LOG_CLASS(lvl) BOOST_LOG_SEV(m_logger, boost::log::trivial::lvl)

#define SET_THREAD_NAME(name) \
static int threadNumber = 0; \
boost::log::core::get()->add_thread_attribute("ThreadName", boost::log::attributes::constant<std::string>(name)); \
boost::log::core::get()->add_thread_attribute("ThreadNumber", boost::log::attributes::constant<int>(++threadNumber));

using severity_level = boost::log::trivial::severity_level;

class Logger : public boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> {
public:
    Logger(const std::string& className = "", const std::string& id = "")
    {
        setClassName(className);
        setId(id);
    }

    void setClassName(const std::string& className)
    {
        if (!className.empty()) {
            add_attribute("Class", boost::log::attributes::constant<std::string>(className));
        }
    }

    void setId(const std::string& id)
    {
        if (!id.empty()) {
            add_attribute("ID", boost::log::attributes::constant<std::string>(id));
        }
    }
};

}
