#pragma once

namespace boost {

inline void assertion_failed_msg(char const* expr, char const* msg, char const* function, char const* /*file*/,
                                 long /*line*/)
{
    std::stringstream error;
    error << "Expression '" << expr << "' is false in function '" << function << "': " << (msg ? msg : "<...>")
        << ".\n" << "Backtrace:\n" << boost::stacktrace::stacktrace();
    std::cerr << error.str() << std::endl;
    BOOST_LOG_TRIVIAL(fatal) << error.str();
    std::cerr.flush();
    std::clog.flush();
    std::cout.flush();

    throw std::runtime_error(error.str());
}

inline void assertion_failed(char const* expr, char const* function, char const* file, long line)
{
    ::boost::assertion_failed_msg(expr, nullptr, function, file, line);
}

}
