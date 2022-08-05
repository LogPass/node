#pragma once

struct DatabaseOptions {
    size_t maxOpenFiles = 32768;
    size_t cacheSize = 8192;

    static program_options::options_description getOptionsDescription();
    static DatabaseOptions loadOptions(program_options::variables_map& optionsVariableMap);
};
