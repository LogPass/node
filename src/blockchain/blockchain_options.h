#pragma once

#include "block/block.h"

#include <filesystem/filesystem.h>

namespace logpass {

struct BlockchainOptions {
    PrivateKey minerKey;
    std::map<uint32_t, Block_cptr> firstBlocks;
    bool initialize = false;
    size_t threads = 8;

    static program_options::options_description getOptionsDescription();

    static BlockchainOptions loadOptions(program_options::variables_map& optionsVariableMap, Filesystem& fs);

};

}
