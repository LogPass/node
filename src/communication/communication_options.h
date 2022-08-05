#pragma once

#include <filesystem/filesystem.h>

namespace logpass {

struct CommunicationOptions {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
    std::map<MinerId, Endpoint> trustedNodes;

    static program_options::options_description getOptionsDescription();

    static CommunicationOptions loadOptions(program_options::variables_map& optionsVariableMap, Filesystem& fs);

};

}
