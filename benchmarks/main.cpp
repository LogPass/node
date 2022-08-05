#include "pch.h"

#define BOOST_TEST_MAIN
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#define BOOST_TEST_MODULE Benchmarks

#include <boost/test/unit_test.hpp>
#ifndef _MSC_VER
#include <boost/test/included/unit_test.hpp>
#endif

int main(int argc, char* argv[])
{
    SET_THREAD_NAME("main");
    boost::log::core::get()->set_filter
    (
        boost::log::trivial::severity >= boost::log::trivial::debug
    );

    namespace keywords = boost::log::keywords;
    namespace expr = boost::log::expressions;

    // Construct the sink
    typedef logging::sinks::synchronous_sink<logging::sinks::text_ostream_backend> text_sink;
    boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

    // Add a stream to write log to
    sink->locked_backend()->add_stream(boost::make_shared<std::ofstream>("benchmark.log"));

    sink->set_formatter
    (
        expr::stream <<
        "[" << expr::format_date_time(expr::attr<boost::posix_time::ptime>("TimeStamp"), "%Y-%m-%d %H:%M:%S.%f") <<
        "] [" << std::setw(8) << std::setfill('0') << expr::attr<boost::log::thread_id>("ThreadID") <<
        "] [" << std::setw(13) << std::setfill(' ') << expr::attr<std::string>("ThreadName") <<
        "-" << std::setw(3) << std::setfill('0') << expr::attr<int>("ThreadNumber") <<
        "] [" << std::setw(7) << std::setfill(' ') << expr::attr< boost::log::trivial::severity_level>("Severity") <<
        "] [" << expr::attr<std::string>("Class") <<
        "] [" << expr::attr<std::string>("ID") <<
        "] " << expr::message
    );

    logging::core::get()->add_sink(sink);
    logging::add_common_attributes();

    return boost::unit_test::unit_test_main(init_unit_test, argc, argv);
}
