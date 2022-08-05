#include "pch.h"

#include "communication_options.h"

namespace po = program_options;

namespace logpass {

po::options_description CommunicationOptions::getOptionsDescription()
{
    po::options_description options("Communication options");

    options.add_options()
        ("host", po::value<std::string>()->default_value("0.0.0.0"), "host of blockchain communication service")
        ("port", po::value<uint16_t>()->default_value(8150), "port of blockchain communication service (0=disabled)")
        ("trusted-nodes-file", po::value<std::string>()->default_value(""), "path to json file with trusted nodes");

    return options;
}

CommunicationOptions CommunicationOptions::loadOptions(po::variables_map& vm, Filesystem& fs)
{
    CommunicationOptions options;
    options.host = vm["host"].as<std::string>();
    options.port = vm["port"].as<uint16_t>();

    std::string trustedNodesFile = vm["trusted-nodes-file"].as<std::string>();
    if (!trustedNodesFile.empty()) {
        auto trustedNodes = fs.readJSON(trustedNodesFile);
        if (!trustedNodes) {
            THROW_EXCEPTION(po::error("Can not read first blocks file: "s + trustedNodesFile));
        }
        try {
            for (auto& entry : trustedNodes->items()) {
                options.trustedNodes.emplace(entry.key(), entry.value());
            }
        } catch (const json::exception& exception) {
            THROW_EXCEPTION(po::error("Json parse error (trusted nodes): "s + exception.what()));
        }
    }

    return options;
}

}
