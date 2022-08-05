#include "pch.h"

#include "blockchain_options.h"

namespace po = program_options;

namespace logpass {

po::options_description BlockchainOptions::getOptionsDescription()
{
    po::options_description options("Blockchain options");
    po::options_description advancedOptions("Advanced blockchain options");

    options.add_options()
        ("key-file", po::value<std::string>()->default_value("key.pem"),
         "path to file with ed25519 private key in PEM format")
        ("key-file-password", po::value<std::string>()->default_value(""),
         "password to decrypt key-file")
        ("first-blocks-file", po::value<std::string>()->default_value(""),
         "path to file with first blocks")
        ("initialize", po::bool_switch()->default_value(false),
         "initialize new blockchain if it doesn't exists");

    advancedOptions.add_options()
        ("threads", po::value<size_t>()->default_value(8),
         "number of threads used by blockchain instance");

    options.add(advancedOptions);
    return options;
}

BlockchainOptions BlockchainOptions::loadOptions(po::variables_map& vm, Filesystem& fs)
{
    std::string keyFile = vm["key-file"].as<std::string>();
    std::string keyPassword = vm["key-file-password"].as<std::string>();
    std::string firstBlocksFile = vm["first-blocks-file"].as<std::string>();
    
    BlockchainOptions options;

    if (!firstBlocksFile.empty()) {
        auto firstBlocks = fs.readJSON(firstBlocksFile);
        if (!firstBlocks) {
            THROW_EXCEPTION(po::error("Can not read first blocks file: "s + firstBlocksFile));
        }
        try {
            for (uint32_t blockId = 0; auto & serializedBlock : *firstBlocks) {
                auto block = std::make_shared<Block>();
                Serializer::fromBase64(serializedBlock.get<std::string>()).serialize(block);
                if (blockId >= block->getId()) {
                    THROW_EXCEPTION(po::error("Invalid block: "s + block->toString()));
                }
                options.firstBlocks.emplace(block->getId(), block);
                blockId = block->getId();
            }
        } catch (const json::exception& exception) {
            THROW_EXCEPTION(po::error("Json parse error (first blocks): "s + exception.what()));
        }
    }

    try {
        auto key = fs.read(keyFile);
        if (!key) {
            THROW_EXCEPTION(po::error("Can not read private key from file: "s + firstBlocksFile));
        }
        options.minerKey = PrivateKey::fromPEM(*key, keyPassword);
    } catch (const CryptoException& exception) {
        THROW_EXCEPTION(po::error("Can not read private key from "s + keyFile + " ("s + exception.what() + ")"s));
    }

    options.initialize = vm["initialize"].as<bool>();
    options.threads = vm["threads"].as<size_t>();

    return options;
}

}
