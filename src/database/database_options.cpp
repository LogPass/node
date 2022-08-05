#include "pch.h"

#include "database_options.h"

namespace po = program_options;

po::options_description DatabaseOptions::getOptionsDescription()
{
    po::options_description options("Database options");

    options.add_options()
        ("max-open-files", po::value<size_t>()->default_value(32768), "number of max opened files by database")
        ("cache-size", po::value<size_t>()->default_value(8192), "max cache size in MBs for database");

    return options;
}

DatabaseOptions DatabaseOptions::loadOptions(po::variables_map& vm)
{
    DatabaseOptions options;
    options.maxOpenFiles = vm["max-open-files"].as<size_t>();
    options.cacheSize = vm["cache-size"].as<size_t>();
    return options;
}
