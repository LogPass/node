#pragma once

#include <communication/communication_test.h>
#include <blockchain/blockchain_fixture.h>

namespace logpass {

// used by BOOST_FIXTURE_TEST_SUITE and BOOST_FIXTURE_TEST_CASE
struct CommunicationFixture {
    CommunicationFixture()
    {

    }

    ~CommunicationFixture()
    {
        users.clear();
        for (size_t i = 0; i < communications.size(); ++i) {
            communications[i].reset();
            blockchains[i].reset();
            databases[i].reset();
            fixtures[i].reset();
        }
    }

    CommunicationFixture(const CommunicationFixture&) = delete;
    CommunicationFixture& operator = (const CommunicationFixture&) = delete;

    void addInstances(size_t size, BlockchainOptions blockchainOptions = BlockchainOptions(),
                      CommunicationOptions communicationOptions = CommunicationOptions(), bool addNewBlocks = true,
                      size_t newMinersStake = kFirstUserStake)
    {
        ASSERT(size > 0);

        auto keys = PrivateKey::generate(size);

        // create first instance if doesn't exists
        if (communications.empty()) {
            blockchainOptions.minerKey = keys[0];
            blockchainOptions.initialize = true;
            fixtures.push_back(std::make_shared<BlockchainFixture>(blockchainOptions));
            blockchains.push_back(fixtures.back()->blockchain);
            databases.push_back(fixtures.back()->db);
            users.push_back(fixtures.back()->user);
            MinerSettings settings;
            settings.endpoint = Endpoint("127.0.0.1", m_port);
            users[0].updateMiner(MinerId(keys[0].publicKey()), settings);
        }

        // create new miners
        for (size_t i = communications.empty() ? 1 : 0; i < size; ++i) {
            users.emplace_back(users[0].createUser(keys[i]));
            users[0].transferTokens(users[i], newMinersStake + kTransactionFee * 10000);
            MinerId minerId(keys[i].publicKey());
            users[i].createMiner(minerId);
            users[i].selectMiner(minerId);
            if (newMinersStake > 0) {
                users[i].increaseStake(minerId, newMinersStake);
            }
            MinerSettings settings;
            settings.endpoint = Endpoint("127.0.0.1", m_port + i);
            users[i].updateMiner(minerId, settings);
            ASSERT(databases[0]->unconfirmed().miners.getMiner(minerId)->stake == kFirstUserStake);
        }

        // commit transactions
        Block_cptr block = blockchains[0]->mineBlock();
        if (addNewBlocks) {
            for (auto& blockchain : blockchains) {
                blockchain->addBlock(block);
            }
        } else {
            blockchains[0]->addBlock(block);
        }

        // load blocks
        block = blockchains[0]->getBlock(1);
        std::map<uint32_t, Block_cptr> blocks = { { block->getId() , block } };
        while (block && addNewBlocks) {
            block = blockchains[0]->getBlockAfter(block->getId());
            if (!block) {
                break;
            }
            blocks.emplace(block->getId(), block);
        }
        blockchainOptions.firstBlocks = blocks;

        // create new blockchains
        size_t startIndex = communications.empty() ? 1 : 0;
        for (size_t i = startIndex; i < size; ++i) {
            blockchainOptions.minerKey = keys[i];
            fixtures.push_back(std::make_shared<BlockchainFixture>(blockchainOptions));
            blockchains.push_back(fixtures.back()->blockchain);
            databases.push_back(fixtures.back()->db);
            blockchains.back()->setExpectedBlockId(blockchains[0]->getLatestBlockId());
        }

        // create new communications
        for (size_t i = 0; i < size; ++i) {
            communicationOptions.host = "127.0.0.1";
            communicationOptions.port = m_port + i;
            size_t idx = communications.size();
            auto blockchain = std::dynamic_pointer_cast<Blockchain>((std::shared_ptr<BlockchainTest>)blockchains[idx]);
            communications.push_back(SharedThread<CommunicationTest>(communicationOptions, blockchain, databases[idx]));
        }

        m_port += size;
    }

    bool mineAndAddBlock(uint32_t blockId, std::vector<Transaction_cptr> extraTransactions = {})
    {
        MinerId minerId;
        for (size_t i = 0; i < blockchains.size(); ++i) {
            if (!blockchains[i]) {
                continue;
            }
            if (blockId <= blockchains[i]->getLatestBlockId()) {
                continue;
            }
            if (blockId > blockchains[i]->getLatestBlockId() + kMinersQueueSize) {
                continue;
            }
            auto miningQueue = databases[i]->confirmed().blocks.getMinersQueue();
            minerId = miningQueue[blockId - (blockchains[i]->getLatestBlockId() + 1)];
            break;
        }

        if (!minerId.isValid()) {
            return false;
        }

        bool blockAdded = false;
        for (auto& blockchain : blockchains) {
            if (blockchain->getMinerId() != minerId) {
                continue;
            }
            if (blockId <= blockchain->getLatestBlockId()) {
                break;
            }
            if (blockId > blockchain->getLatestBlockId() + kMinersQueueSize) {
                break;
            }

            if (!extraTransactions.empty() && !blockchain->postTransactions(extraTransactions)) {
                return false;
            }

            size_t blocksToSkip = blockId - (blockchain->getLatestBlockId() + 1);
            auto block = blockchain->mineBlock(blocksToSkip);
            blockchain->addBlock(block);
            blockAdded = true;
        }

        if (!blockAdded) {
            return false;
        }

        for (auto& blockchain : blockchains) {
            blockchain->setExpectedBlockId(blockId);
        }

        return true;
    }

    // maxTimeout is in seconds, max 300s
    bool synchronizeInstances(size_t maxTimeout)
    {
        ASSERT(maxTimeout > 0 && maxTimeout <= 300);
        Block_cptr deepestBlock = nullptr;
        bool duplicatedDepth = false;
        for (auto& blockchain : blockchains) {
            Block_cptr lastBlock = blockchain->getBlockTree().getActiveBranch().back().block;
            if (!deepestBlock || lastBlock->getDepth() > deepestBlock->getDepth()) {
                deepestBlock = lastBlock;
                duplicatedDepth = false;
            } else if (lastBlock->getDepth() == deepestBlock->getDepth() &&
                       lastBlock->getHeaderHash() != deepestBlock->getHeaderHash()) {
                duplicatedDepth = true;
            }
        }

        // there are no clear winner when it comes to deepestBlock, so instances cannot be synchronized
        if (duplicatedDepth) {
            return false;
        }

        if (deepestBlock) {
            for (auto& blockchain : blockchains) {
                blockchain->setExpectedBlockId(deepestBlock->getId());
            }
        }

        auto deadline = chrono::high_resolution_clock::now() + chrono::seconds(maxTimeout);
        for (size_t i = 0; i < maxTimeout * 100; ++i) {
            if (chrono::high_resolution_clock::now() >= deadline) {
                return false;
            }
            for (auto& communication : communications) {
                communication->check(false);
            }
            for (auto& blockchain : blockchains) {
                blockchain->update(false);
            }

            size_t blockId = blockchains[0]->getLatestBlockId();
            bool isSynchronized = true;
            for (auto& blockchain : blockchains) {
                if (blockId != blockchain->getLatestBlockId()) {
                    isSynchronized = false;
                    break;
                }
            }
            if (!isSynchronized) {
                std::this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }
            return true;
        }
        return false;
    }

    // maxTimeout is in seconds, max 300s
    bool synchronizeTransactions(size_t maxTimeout, const std::vector<Transaction_cptr>& transactions,
                                 bool confirmed = false)
    {
        ASSERT(maxTimeout > 0 && maxTimeout <= 300);
        auto deadline = chrono::high_resolution_clock::now() + chrono::seconds(maxTimeout);
        for (size_t i = 0; i < maxTimeout * 100; ++i) {
            if (chrono::high_resolution_clock::now() >= deadline) {
                return false;
            }
            for (auto& communication : communications) {
                communication->check(false);
            }
            for (auto& blockchain : blockchains) {
                blockchain->update(false);
            }

            bool isSynchronized = true;
            for (auto& transaction : transactions) {
                for (auto& database : databases) {
                    if (confirmed) {
                        if (!database->confirmed().transactions.getTransaction(transaction->getId())) {
                            isSynchronized = false;
                        }
                    } else {
                        if (!database->unconfirmed().transactions.getTransaction(transaction->getId())) {
                            isSynchronized = false;
                        }
                    }
                }
                if (!isSynchronized) {
                    break;
                }
            }
            if (!isSynchronized) {
                std::this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }
            return true;
        }
        return false;
    }

    uint16_t m_port = 30050;
    std::vector<std::shared_ptr<BlockchainFixture>> fixtures;
    std::vector<std::shared_ptr<Database>> databases;
    std::vector<std::shared_ptr<BlockchainTest>> blockchains;
    std::vector<SharedThread<CommunicationTest>> communications;
    std::vector<TestUser> users;
};

}
