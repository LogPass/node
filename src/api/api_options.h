#pragma once

namespace logpass {

struct ApiOptions {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;

    static program_options::options_description getOptionsDescription();
    static ApiOptions loadOptions(program_options::variables_map& optionsVariableMap);
};

}
