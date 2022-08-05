#include "pch.h"

#include <boost/test/unit_test.hpp>

#include <filesystem/filesystem.h>
#include <api/api.h>

#include <blockchain/blockchain_fixture.h>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/transfer.h>
#include <blockchain/transactions/storage/add_entry.h>
#include <blockchain/transactions/storage/create_prefix.h>

#include "time_tester.h"

using namespace logpass;
BOOST_AUTO_TEST_CASE(blockchain)
{
    auto& argv = boost::unit_test::framework::master_test_suite().argv;
    auto& argc = boost::unit_test::framework::master_test_suite().argc;
    std::vector<std::string> args(argv + 1, argv + argc);
    bool useExistingDatabase = std::find(args.begin(), args.end(), "--use-existing-database") != args.end();
    if (useExistingDatabase) {
        BOOST_TEST_MESSAGE("Using existing database for blockchain benchmark");
    }

    PrivateKey minerKey = PrivateKey::generate();
    MinerId minerId = MinerId(minerKey.publicKey());

    SharedThread<Filesystem> filesystem;
    if (useExistingDatabase) {
        filesystem = SharedThread<Filesystem>(".");
    } else {
        filesystem = Filesystem::createTemporaryFilesystem();
    }

    if (!useExistingDatabase) {
        std::vector<TransactionId> transactionIds;
        auto database = SharedThread<Database>(DatabaseOptions(), filesystem);
        BlockchainOptions blockchainOptions;
        blockchainOptions.minerKey = minerKey;
        blockchainOptions.initialize = true;
        blockchainOptions.threads = 8;
        auto blockchain = SharedThread<BlockchainTest>(blockchainOptions, database);

        std::vector<PrivateKey> privateKeys;
        std::vector<PublicKey> publicKeys;
        {
            TimeTester t("Genereting 10k private keys");
            for (int i = 0; i < 10'000; ++i) {
                privateKeys.push_back(PrivateKey::generate());
                publicKeys.push_back(privateKeys.back().publicKey());
            }
        }

        std::vector<Transaction_cptr> newTransactions;
        {
            TimeTester t("Creating 10k and signing create user transactions");
            uint32_t blockId = blockchain->getExpectedBlockId();
            for (size_t i = 0; i < privateKeys.size(); ++i) {
                auto newUserTransaction =
                    CreateUserTransaction::create(blockId, -1, publicKeys[i], kUserMinFreeTransactions)->
                    setUserId(blockchain->getUserId())->sign(blockchain->getMinerKey());
                newTransactions.push_back(newUserTransaction);
            }
        }
        {
            TimeTester t("Posting 10k create user transactions");
            BOOST_TEST_REQUIRE(blockchain->postTransactions(newTransactions));
        }

        Block_cptr block;
        {
            TimeTester t("Mining block with 10k create user transactions");
            block = blockchain->mineBlock();
        }

        {
            TimeTester t("Adding block with 10k create user transactions");
            blockchain->addBlock(block);
        }

        const size_t chunks = 8;
        std::vector<Transaction_cptr> newTransactionsChunks[chunks];
        {
            TimeTester t("Creating 70k * 8 and signing transfer transactions, using 8 threads");
            std::vector<std::thread> threads;
            uint32_t blockId = blockchain->getExpectedBlockId();
            for (size_t x = 0; x < chunks; ++x) {
                threads.push_back(std::thread([&, x] {
                    for (size_t i = x; i < 70000 * chunks; i += chunks) {
                        auto transferTransaction =
                            TransferTransaction::create(blockId, -1, publicKeys[i % privateKeys.size()],
                                                        1 + i / privateKeys.size())->setUserId(blockchain->getUserId())->
                            sign({ blockchain->getMinerKey() });
                        newTransactionsChunks[x].push_back(transferTransaction);
                    }
                }));
            }
            for (auto& thread : threads) {
                thread.join();
            }
        }

        for (auto& chunk : newTransactionsChunks) {
            for (auto& newTransaction : chunk) {
                transactionIds.push_back(newTransaction->getId());
            }

            {
                TimeTester t("Posting 70k transfer transactions");
                BOOST_TEST_REQUIRE(blockchain->postTransactions(chunk));
            }

            while (blockchain->getPendingTransactions().getPendingTransactionsCount() > 10000) {
                {
                    TimeTester t("Mining block with transfer transactions");
                    block = blockchain->mineBlock();
                }

                {
                    TimeTester t("Adding block with transfer transactions");
                    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
                }
            }
        }

        auto postTransactionsOnThreads = [&](size_t transactions) {
            std::vector<std::thread> threads;
            std::vector<Transaction_cptr> newTransactions[8];
            uint32_t blockId = blockchain->getExpectedBlockId();
            for (size_t x = 0; x < 8; ++x) {
                threads.push_back(std::thread([&, x] {
                    for (size_t i = x; i < transactions; i += 8) {
                        auto transferTransaction =
                            TransferTransaction::create(blockId, -1, publicKeys[i % privateKeys.size()],
                                                        1 + i / privateKeys.size())->setUserId(blockchain->getUserId())->
                            sign({ blockchain->getMinerKey() });
                        newTransactions[x].push_back(transferTransaction);
                    }
                }));
            }
            for (size_t x = 0; x < 8; ++x) {
                threads[x].join();
                BOOST_TEST_REQUIRE(blockchain->postTransactions(newTransactions[x]));
            }
        };

        {
            for (size_t i = 0; i < 30; ++i) {
                {
                    TimeTester t("Generating and posting 10k transactions "s + std::to_string(i) + "/30");
                    postTransactionsOnThreads(10000);
                }
                TimeTester t("Adding block with 10k transaction "s + std::to_string(i) + "/30");
                BOOST_TEST_REQUIRE(blockchain->addBlock(blockchain->mineBlock()));
            }
        }

        {
            TimeTester t("Getting 70k * 8 transfer transactions from database");
            for (auto& id : transactionIds) {
                if (database->confirmed().transactions.getTransaction(id)->getId() != id)
                    BOOST_FAIL("Invalid id");
            }
        }
    }

    {
        SharedThread<Database> database;
        {
            TimeTester t("Initializing database");
            database = SharedThread<Database>(DatabaseOptions(), filesystem);
        }

        const int maxTransactions = 1'000'000;
        uint32_t latestBlockId = database->confirmed().blocks.getLatestBlockId();
        std::set<TransactionId> transactionsToFindSet;
        {
            TimeTester t("Getting transactions ids from "s + std::to_string(latestBlockId) + " blocks");
            for (int x = 0; x < 64 && transactionsToFindSet.size() < maxTransactions; ++x) {
                for (uint32_t i = 1; i <= latestBlockId && transactionsToFindSet.size() < maxTransactions; ++i) {
                    auto blockBody = database->confirmed().blocks.getBlockBody(i);
                    if (!blockBody)
                        continue;
                    auto BlockTransactionIds = database->confirmed().blocks.getBlockTransactionIds(i, rand() % blockBody->getHash().size());
                    if (!BlockTransactionIds)
                        continue;
                    for (auto& it : *BlockTransactionIds) {
                        transactionsToFindSet.insert(it);
                    }
                }
            }
        }
        BOOST_TEST_MESSAGE("Found " << transactionsToFindSet.size() << " transactions");

        std::vector<TransactionId> transactionsToFind(transactionsToFindSet.begin(), transactionsToFindSet.end());
        {
            TimeTester t("Doing shuffle and limiting");
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(transactionsToFind.begin(), transactionsToFind.end(), g);
            if (transactionsToFind.size() > maxTransactions)
                transactionsToFind.resize(maxTransactions);
        }

        // reinitialize database for clear stats
        database.reset();
        database = SharedThread<Database>(DatabaseOptions(), filesystem);

        {
            TimeTester t("Getting "s + std::to_string(transactionsToFind.size()) + " transactions from database (8 threads)");
            std::vector<std::thread> threads;
            for (size_t x = 0; x < 8; ++x) {
                threads.push_back(std::thread([&, x] {
                    for (size_t i = x; i < transactionsToFind.size(); i += 8) {
                        auto& transactionId = transactionsToFind[i];
                        if (database->confirmed().transactions.getTransaction(transactionId)->getId() != transactionId)
                            BOOST_TEST_FAIL("Invalid transaction id");
                    }
                }));
            }
            for (auto& thread : threads) {
                thread.join();
            }
        }

        database.reset();
        database = SharedThread<Database>(DatabaseOptions(), filesystem);

        {
            TimeTester t("Getting "s + std::to_string(transactionsToFind.size()) + " transactions from database (single thread)");
            for (auto& transactionId : transactionsToFind) {
                if (database->confirmed().transactions.getTransaction(transactionId)->getId() != transactionId)
                    BOOST_TEST_FAIL("Invalid transaction id");
            }
        }

        {
            TimeTester t("Getting "s + std::to_string(transactionsToFind.size()) + " transactions from database again");
            for (auto& transactionId : transactionsToFind) {
                if (database->confirmed().transactions.getTransaction(transactionId)->getId() != transactionId)
                    BOOST_TEST_FAIL("Invalid transaction id");
            }
        }

        {
            TimeTester t("Getting "s + std::to_string(maxTransactions) + " invalid transactions from database");
            for (size_t i = 0; i < maxTransactions; ++i) {
                if (database->confirmed().transactions.getTransaction(TransactionId(i % 10, i, i, Hash::generate(std::to_string(i)))))
                    BOOST_TEST_FAIL("Transaction shouldn't exist");
            }
        }
    }
}

BOOST_FIXTURE_TEST_CASE(slowest_transactions, BlockchainFixture)
{
    std::vector<TestUser> users;
    auto keys = PrivateKey::generate(1000);

    {
        TimeTester t("Initializing users"s);
        for (size_t i = 0; i < 1000; ++i) {
            users.push_back(user.createUser(keys[i]));
            BOOST_TEST_REQUIRE(user.transferTokens(users.back(), 100000 * kTransactionFee));
            BOOST_TEST_REQUIRE(users.back().createMiner(keys[i].publicKey()));
            BOOST_TEST_REQUIRE(users.back().selectMiner(keys[i].publicKey()));
        }
        BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    }

    {
        TimeTester t("Updating users"s);
        for (size_t i = 0; i < 1000; ++i) {
            UserSettings settings;
            std::vector<PrivateKey> signingKeys;
            std::map<PublicKey, uint8_t> publicKeys;
            for (size_t x = 0; x < kUserMaxKeys; ++x) {
                signingKeys.push_back(keys[(i + x) % 1000]);
                publicKeys.emplace(keys[(i + x) % 1000].publicKey(), 1);
            }
            std::map<UserId, uint8_t> supervisors;
            for (size_t x = 0; x < kUserMaxSupervisors; ++x) {
                signingKeys.push_back(keys[(i + x + 100) % 1000]);
                supervisors.emplace(keys[(i + x + 100) % 1000].publicKey(), 1);
            }

            settings.keys = UserKeys::create(publicKeys);
            settings.supervisors = UserSupervisors::create(supervisors);
            settings.rules.powerLevels.fill(kUserMaxKeys + kUserMaxSupervisors);
            settings.rules.powerLevels[0] = 1;
            BOOST_TEST_REQUIRE(users[i].updateUser(settings));
            users[i].setKeys(signingKeys);
        }
        BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    }

    for (size_t blockId = 1; blockId <= 100; ++blockId) {
        std::vector<Transaction_cptr> transactions(8000, nullptr);
        {
            TimeTester t("Creating 8k transfer transactions"s);
            std::vector<std::thread> threads;
            for (size_t iter = 0; iter < 8; ++iter) {
                threads.emplace_back([&, iter] {
                    for (size_t i = 0; i < 1000; ++i) {
                        UserId recipient = users[(i + 1 + iter) % 1000];
                        UserId sponsor = keys[(i + 100) % 1000].publicKey();
                        auto transaction = TransferTransaction::create(blockId, 1, recipient, kTransactionFee + iter)->
                            setUserId(users[i])->setSponsorId(sponsor)->sign(users[i].getKeys());
                        transactions[i + iter * 1000] = transaction;
                    }
                });
            }
            for (auto& thread : threads) {
                thread.join();
            }
        }

        {
            TimeTester t("Posting 8k transfer transactions"s);
            BOOST_TEST_REQUIRE(blockchain->postTransactions(transactions));
        }

        Block_cptr block;
        {
            TimeTester t("Mining block with 8k transfer transactions (block "s + std::to_string(blockId) + ")"s);
            block = blockchain->mineBlock();
        }

        /*
        {
            TimeTester t("Reinitializing blockchain");
            reinitialize({
                .minerKey = blockchain->getMinerKey()
                         });
        }
        */

        {
            TimeTester t("Adding block with 8k transfer transactions");
            BOOST_TEST_REQUIRE(blockchain->addBlock(block));
        }
    }
}
BOOST_FIXTURE_TEST_CASE(storage_big_transactions, BlockchainFixture)
{
    auto createPrefixTransaction = StorageCreatePrefixTransaction::create(blockchain->getExpectedBlockId(), -1, "XXXX")->
        setUserId(blockchain->getUserId())->sign({ blockchain->getMinerKey() });
    BOOST_TEST_REQUIRE(blockchain->postTransaction(createPrefixTransaction));

    std::string maxStorageValue = "";
    for (size_t i = 0; i < kStorageEntryMaxValueLength; ++i)
        maxStorageValue += (char)(i % 256);

    size_t transactionsToExecute = blockchain->getPendingTransactions().getMaxPendingTransactionsSize() / kTransactionMaxSize;
    std::vector<Transaction_cptr> transactions;
    {
        TimeTester t("Creating "s + std::to_string(transactionsToExecute) + " big storage transactions"s);
        for (size_t i = 0; i < transactionsToExecute; ++i) {
            auto transaction = StorageAddEntryTransaction::create(blockchain->getExpectedBlockId(), -1, "XXXX",
                                                                  Hash::generateRandom().toString(),
                                                                  maxStorageValue)->
                setUserId(blockchain->getUserId())->sign({ blockchain->getMinerKey() });
            transactions.push_back(transaction);
        }
    }

    {
        TimeTester t("Executing "s + std::to_string(transactionsToExecute) + " big storage transactions (with deserialization)"s);
        for (auto& transaction : transactions) {
            Serializer s;
            s(transaction);
            s.switchToReader();
            BOOST_TEST_REQUIRE(blockchain->postTransaction(s));
        }
    }

    {
        TimeTester t("Executing blocks"s);
        for (size_t i = 0; i < transactions.size(); ++i) {
            BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
            if (db->confirmed().transactions.getTransaction(transactions.back()->getId()))
                break;
        }
    }

    {
        TimeTester t("Getting transactions"s);
        for (auto& transaction : transactions) {
            BOOST_TEST_REQUIRE(db->confirmed().transactions.getTransaction(transaction->getId()));
        }
    }
}

BOOST_FIXTURE_TEST_CASE(maximum_transactions_count, BlockchainFixture)
{
    std::vector<TransactionId> transactionIds;
    for (size_t i = 0; i < kBlockMaxTransactions - 1; ++i) {
        auto status = user.createUser(PublicKey::generateRandom());
        BOOST_TEST_REQUIRE(status);
        transactionIds.push_back(status.getTransactionId());
    }
    auto block = blockchain->mineBlock();
    BOOST_TEST_REQUIRE(block->getTransactions() == kBlockMaxTransactions);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    for (auto& transactionId : transactionIds) {
        BOOST_TEST_REQUIRE(db->confirmed().transactions.getTransaction(transactionId));
    }
}

BOOST_FIXTURE_TEST_CASE(maximum_transactions_size, BlockchainFixture)
{
    user.createPrefix("AAAA");
    uint64_t transactionsSize = 0;
    std::vector<TransactionId> transactionIds;
    for (size_t i = 0; i < kBlockMaxTransactions; ++i) {
        std::string randomVal;
        randomVal.resize(1500);
        randomVal += std::to_string(i);
        auto status = user.addStorageEntry("AAAA", std::to_string(i), randomVal);
        BOOST_TEST_REQUIRE(status);
        BOOST_TEST_REQUIRE(status.getTransactionId().getSize() < 2048);
        transactionIds.push_back(status.getTransactionId());
        transactionsSize += status.getTransactionId().getSize();
        if (transactionsSize > kBlockMaxTransactionsSize - 2048) {
            transactionIds.pop_back();
            break;
        }
    }

    auto block = blockchain->mineBlock();
    BOOST_TEST_REQUIRE(block->getTransactionsSize() > kBlockMaxTransactionsSize - 2048);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    for (auto& transactionId : transactionIds) {
        BOOST_TEST_REQUIRE(db->confirmed().transactions.getTransaction(transactionId));
    }
}
