#include "pch.h"

#include "api_options.h"

namespace po = program_options;

namespace logpass {

po::options_description ApiOptions::getOptionsDescription()
{
    po::options_description options("API options");

    options.add_options()
        ("api-host", po::value<std::string>()->default_value("0.0.0.0"), "host of api service")
        ("api-port", po::value<uint16_t>()->default_value(8080), "port of api service (0=disabled)");

    return options;
}

ApiOptions ApiOptions::loadOptions(po::variables_map& vm)
{
    ApiOptions options;
    options.host = vm["api-host"].as<std::string>();
    options.port = vm["api-port"].as<uint16_t>();

    return options;
}

}
