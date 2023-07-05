#include "pch.h"

#include <boost/test/unit_test.hpp>

#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/transfer.h>
#include <blockchain/transactions/storage/add_entry.h>
#include <blockchain/transactions/storage/create_prefix.h>
#include <communication/communication_fixture.h>

#include "time_tester.h"

using namespace logpass;
BOOST_FIXTURE_TEST_CASE(multiple_instances, CommunicationFixture)
{
    auto privateKeys = PrivateKey::generate(10000);
    std::vector<PublicKey> publicKeys;
    for (auto& privateKey : privateKeys)
        publicKeys.push_back(privateKey.publicKey());

    {
        TimeTester p("Adding 10 instances");
        addInstances(10);
    }

    // mine kMinersQueueSize blocks to change next miner
    mineAndAddBlock(blockchains[0]->getLatestBlockId() + kMinersQueueSize);
    BOOST_TEST_REQUIRE(databases[0]->confirmed().blocks.getMinersQueue().front() != blockchains[0]->getMinerId());

    BOOST_TEST_REQUIRE(synchronizeInstances(120));

    std::vector<Transaction_cptr> newTransactions;
    for (size_t i = 0; i < privateKeys.size(); ++i) {
        auto transaction = CreateUserTransaction::create(blockchains[0]->getExpectedBlockId(), -1, publicKeys[i],
                                                         kUserMinFreeTransactions)->
            setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });
        newTransactions.push_back(transaction);
    }

    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1, newTransactions));

    {
        TimeTester p("Synchronizing instances");
        BOOST_TEST_REQUIRE(synchronizeInstances(60));
    }

    // desynchrize instances, close all connections, mine 20 + 1 blocks and synchronize instances
    for (auto& communication : communications) {
        communication->closeAllConnections();
    }

    {
        TimeTester p("Mining 21 blocks");
        for (size_t i = 1, blockId = blockchains[0]->getLatestBlockId(); i <= blockchains.size() + 1; ++i) {
            BOOST_TEST_REQUIRE(mineAndAddBlock(blockId + i));
        }
    }

    {
        TimeTester p("Synchronizing instances");
        BOOST_TEST_REQUIRE(synchronizeInstances(60));
    }

    // desynchrize instances again, close all connections, mine a lot of blocks with random transactions 
    // and then synchronize them again
    for (auto& communication : communications) {
        communication->closeAllConnections();
    }

    std::vector<Transaction_cptr> randomTransactions;
    {
        size_t blocksToMine = 1;
        for (size_t step = 1; step <= 5; ++step) {
            blocksToMine += (blockchains.size() - step * 2);
        }

        TimeTester p("Mining "s + std::to_string(blocksToMine) + " blocks with random transactions"s);
        size_t startBlockId = blockchains[0]->getLatestBlockId();
        for (size_t step = 1; step <= 5; ++step) {
            for (size_t i = 1; i <= (blockchains.size() - step * 2); i += 1) {
                uint32_t blockId = startBlockId + (step - 1) * blockchains.size() + i;
                auto transaction = TransferTransaction::create(startBlockId, -1, blockchains[i]->getUserId(), blockId)->
                    setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });
                randomTransactions.push_back(transaction);
                BOOST_TEST_REQUIRE(mineAndAddBlock(blockId, { transaction }));
            }
        }

        // mine one more block to generate longest branch
        BOOST_TEST_REQUIRE(mineAndAddBlock(startBlockId + 5 * blockchains.size() + 1));
    }

    {
        TimeTester p("Synchronizing instances");
        BOOST_TEST_REQUIRE(synchronizeInstances(120));
    }

    {
        TimeTester p("Synchronizing transactions");
        size_t executedRandomTransactions = 0;
        for (size_t i = 0; i < 10; ++i) {
            executedRandomTransactions = 0;
            for (auto& transaction : randomTransactions) {
                if (databases[0]->unconfirmed().transactions.getTransaction(transaction->getId()))
                    executedRandomTransactions += 1;
            }
            if (executedRandomTransactions == randomTransactions.size())
                break;

            BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
            BOOST_TEST_REQUIRE(synchronizeInstances(15));
            std::this_thread::sleep_for(chrono::seconds(1));
        }
        BOOST_TEST_REQUIRE(executedRandomTransactions == randomTransactions.size());
    }
}

BOOST_FIXTURE_TEST_CASE(multiple_instances_big_transactions, CommunicationFixture)
{
    addInstances(1);
    auto createPrefixTransaction =
        StorageCreatePrefixTransaction::create(blockchains[0]->getExpectedBlockId(), -1, "XXXX")->
        setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });

    BOOST_TEST_REQUIRE(blockchains[0]->postTransaction(createPrefixTransaction));
    blockchains[0]->mineBlock();

    {
        TimeTester p("Adding 7 instances");
        addInstances(7);
    }

    std::vector<Transaction_cptr> transactions;
    std::vector<Transaction_cptr> transactionsChunks[10];
    {
        TimeTester p("Generating transactions");
        for (size_t i = 0; i < communications.size(); ++i) {
            for (size_t x = 0, transactionSize = 0; x < kBlockMaxTransactions; ++x) {
                std::string randomValue((1024 * x) % kStorageEntryMaxValueLength, 'X');
                std::string randomKey = Hash::generateRandom().toString();
                auto transaction = StorageAddEntryTransaction::create(blockchains[i]->getExpectedBlockId(), -1,
                                                                      "XXXX", randomKey, randomValue)->
                    setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });
                transactionSize += transaction->getSize();
                transactionsChunks[i].push_back(transaction);
                transactions.push_back(transaction);
                if (transactionSize > kBlockMaxTransactionsSize / 2) {
                    break;
                }
            }
        }
    }

    {
        TimeTester p("Posting "s + std::to_string(transactions.size()) + " transactions"s);
        for (size_t i = 0; i < communications.size(); ++i) {
            BOOST_TEST_REQUIRE(blockchains[i]->postTransactions(transactionsChunks[i]));
        }
    }

    {
        TimeTester p("Synchronizing instances");
        for (size_t i = 0; i < 120; ++i) {
            bool hasTransactions = true;
            for (auto& transaction : transactions) {
                if (!databases[0]->confirmed().transactions.getTransaction(transaction->getId())) {
                    hasTransactions = false;
                    break;
                }
            }
            if (hasTransactions) {
                break;
            }

            BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
            BOOST_TEST_REQUIRE(synchronizeInstances(120));
            std::this_thread::sleep_for(chrono::seconds(1));
        }
    }

    size_t executedRandomTransactions = 0;
    for (auto& transaction : transactions) {
        if (databases[0]->confirmed().transactions.getTransaction(transaction->getId())) {
            executedRandomTransactions += 1;
        }
    }
    BOOST_TEST_REQUIRE(executedRandomTransactions == transactions.size());
}

BOOST_FIXTURE_TEST_CASE(synchronization_100_big_blocks, CommunicationFixture)
{
    addInstances(2);

    {
        TimeTester p("Mining 100 blocks with "s + std::to_string(kBlockMaxTransactions) + " transactions each"s);

        for (size_t blockId = 1; blockId <= 100; ++blockId) {
            size_t transactionsCount = (kBlockMaxTransactions / 8) * 8;
            std::vector<Transaction_cptr> transactions(transactionsCount, nullptr);
            {
                TimeTester t("Creating transfer transactions"s);
                std::vector<std::thread> threads;
                std::atomic_int value = kTransactionFee;
                for (size_t iter = 0; iter < 8; ++iter) {
                    threads.emplace_back([&, iter] {
                        for (size_t i = 0; i < transactionsCount / 8; ++i) {
                            auto transaction = TransferTransaction::create(blockId, -1, users[1], value.fetch_add(1))->
                                setUserId(users[0])->sign(users[0].getKeys());
                            transactions[i + iter * transactionsCount / 8] = transaction;
                        }
                    });
                }
                for (auto& thread : threads) {
                    thread.join();
                }
            }

            {
                TimeTester t("Posting transactions"s);
                BOOST_TEST_REQUIRE(blockchains[0]->postTransactions(transactions));
            }

            Block_cptr block;
            {
                TimeTester t("Mining and adding block (block "s + std::to_string(blockId) + ")"s);
                blockchains[0]->mineAndAddBlock();
            }
        }
    }

    {
        TimeTester p("Synchronizing instances");
        blockchains[1]->setExpectedBlockId(blockchains[0]->getLatestBlockId());
        BOOST_TEST_REQUIRE(synchronizeInstances(600));
    }
}

BOOST_FIXTURE_TEST_CASE(synchronization_1000_small_blocks, CommunicationFixture)
{
    addInstances(1);

    size_t transactionsCount = 128;
    auto newUsers = users[0].createUsers(transactionsCount);

    auto mineBlocks = [&] {
        TimeTester p("Mining 500 blocks with "s + std::to_string(transactionsCount) + " transactions each"s);
        size_t blockId = blockchains[0]->getLatestBlockId();
        for (size_t lastBlockId = blockId + 500; blockId <= lastBlockId; ++blockId) {
            std::vector<Transaction_cptr> transactions;
            {
                TimeTester t("Creating transfer transactions"s);
                for (size_t i = 0; i < transactionsCount; ++i) {
                    auto transaction = TransferTransaction::create(blockId, -1, newUsers[i], kTransactionFee)->
                        setUserId(users[0])->sign(users[0].getKeys());
                    transactions.push_back(transaction);
                }
            }

            {
                TimeTester t("Posting transactions"s);
                BOOST_TEST_REQUIRE(blockchains[0]->postTransactions(transactions));
            }

            {
                TimeTester t("Mining and adding block (block "s + std::to_string(blockId) + ")"s);
                BOOST_TEST_REQUIRE(blockchains[0]->mineAndAddBlock());
            }
        }
    };

    mineBlocks();

    {
        TimeTester p("Adding instances");
        addInstances(9, BlockchainOptions(), CommunicationOptions(), false, 0);
    }

    mineBlocks();

    {
        TimeTester p("Synchronizing instances");
        for (size_t i = 0; i < 100; ++i) {
            if (synchronizeInstances(10)) {
                break;
            }
            BOOST_TEST_MESSAGE("> "s << ((i + 1) * 10) << " seconds from start");
            for (size_t b = 0; b < blockchains.size(); ++b) {
                uint32_t blockId = blockchains[b]->getLatestBlockId();
                uint32_t connections = communications[b]->getActiveConnectionsCount();
                BOOST_TEST_MESSAGE("Blockchain "s << std::setw(2) << b << " has block "s << blockId << " and "s <<
                    connections << " active connections"s);
            }
        }
    }
}

BOOST_FIXTURE_TEST_CASE(storage_benchmark, CommunicationFixture)
{
    {
        TimeTester p("Initializing first instance");
        addInstances(1);
    }
    auto createPrefixTransaction =
        StorageCreatePrefixTransaction::create(blockchains[0]->getExpectedBlockId(), -1, "XXXX")->
        setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });

    BOOST_TEST_REQUIRE(blockchains[0]->postTransaction(createPrefixTransaction));
    blockchains[0]->mineBlock();

    {
        TimeTester p("Adding 7 instances (8 in total)");
        addInstances(7);
    }

    for(size_t i = 0; i < 100; ++i) {
        TimeTester p("Executing 2048 transactions, each with at least 16 KB of data and synchronizing them, round "s + std::to_string(i+1) + " of 100"s);
        std::vector<Transaction_cptr> transactions;

        {
            TimeTester p("Generating 2048 transactions, each with at least 16 KB of data");
            for (size_t x = 0; x < 2048; ++x) {
                std::string randomValue((1024 * 16 + x) % kStorageEntryMaxValueLength, 'X');
                std::string randomKey = Hash::generateRandom().toString();
                auto transaction = StorageAddEntryTransaction::create(blockchains[0]->getExpectedBlockId(), -1,
                                                                        "XXXX", randomKey, randomValue)->
                    setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });
                transactions.push_back(transaction);
            }
        }

        {
            TimeTester p("Posting "s + std::to_string(transactions.size()) + " transactions to random node"s);
            BOOST_TEST_REQUIRE(blockchains[i % communications.size()]->postTransactions(transactions));
        }

        {
            TimeTester p("Synchronizing all instances");
            for (size_t i = 0; i < 120; ++i) {
                bool hasTransactions = true;
                for (auto& transaction : transactions) {
                    if (!databases[0]->confirmed().transactions.getTransaction(transaction->getId())) {
                        hasTransactions = false;
                        break;
                    }
                }
                if (hasTransactions) {
                    break;
                }

                {
                    TimeTester p("Mining new block");
                    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
                }
                BOOST_TEST_REQUIRE(synchronizeInstances(120));
                std::this_thread::sleep_for(chrono::seconds(1));
            }
        }
    }
}