#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/storage/add_entry.h>
#include <blockchain/transactions/storage/create_prefix.h>
#include <communication/packets/get_block_header.h>

#include "communication_fixture.h"

using namespace logpass;


BOOST_FIXTURE_TEST_SUITE(communication, CommunicationFixture);

BOOST_AUTO_TEST_CASE(establish_connection)
{
    addInstances(2);
    std::this_thread::sleep_for(chrono::seconds(1));
    auto c1 = communications[0]->connect(MinerId(PrivateKey::generate().publicKey()),
                                         Endpoint("127.0.0.1", communications[1]->getPort()));
    BOOST_TEST_REQUIRE(!c1);
    auto c2 = communications[0]->connect(blockchains[1]->getMinerId(),
                                         Endpoint("127.0.0.1", communications[1]->getPort()));
    BOOST_TEST_REQUIRE(c2);
    BOOST_TEST_REQUIRE(!communications[0]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[0]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    std::this_thread::sleep_for(chrono::seconds(1));
    auto c3 = communications[1]->getConnectionByMinerId(blockchains[0]->getMinerId());
    BOOST_TEST_REQUIRE(c3);
    BOOST_TEST_REQUIRE(!communications[1]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    communications[1]->post([&] {
        c3->close();
    });
    std::this_thread::sleep_for(chrono::seconds(1));
    BOOST_TEST_REQUIRE(!communications[0]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    BOOST_TEST_REQUIRE(!communications[1]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    auto c4 = communications[1]->connect(MinerId(PublicKey::generateRandom()),
                                         Endpoint("127.0.0.1", communications[0]->getPort()));
    BOOST_TEST_REQUIRE(!c4);
    auto c5 = communications[1]->connect(blockchains[0]->getMinerId(),
                                         Endpoint("127.0.0.1", communications[0]->getPort()));
    BOOST_TEST_REQUIRE(c5);
    BOOST_TEST_REQUIRE(communications[1]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    addInstances(1);
    std::this_thread::sleep_for(chrono::seconds(1));
    auto c6 = communications[0]->connect(blockchains[2]->getMinerId(),
                                         Endpoint("127.0.0.1", communications[2]->getPort()));
    BOOST_TEST_REQUIRE(c6);
    c1 = c2 = c6 = nullptr;
    communications[0].reset();
    for (int i = 0; i < 100; ++i) {
        if (!communications[1]->getConnectionByMinerId(blockchains[0]->getMinerId()) &&
            !communications[2]->getConnectionByMinerId(blockchains[0]->getMinerId()))
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    BOOST_TEST_REQUIRE(!communications[1]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    BOOST_TEST_REQUIRE(!communications[2]->getConnectionByMinerId(blockchains[0]->getMinerId()));
}

BOOST_AUTO_TEST_CASE(reopen_acceptor)
{
    {
        CommunicationFixture instances2;
        instances2.addInstances(1);
        addInstances(1);
        addInstances(1);
        std::this_thread::sleep_for(chrono::seconds(1));
        auto connection = communications[0]->connect(blockchains[0]->getMinerId(),
                                                     Endpoint("127.0.0.1", communications[1]->getPort()));
        BOOST_TEST_REQUIRE(!connection);
    }
    bool isValid = false;
    std::this_thread::sleep_for(chrono::seconds(2));
    for (int i = 0; i < 10; ++i) {
        auto connection = communications[0]->connect(blockchains[1]->getMinerId(),
                                                     Endpoint("127.0.0.1", communications[1]->getPort()));
        if (connection) {
            isValid = true;
            break;
        }
        std::this_thread::sleep_for(chrono::seconds(1));
    }
    BOOST_TEST_REQUIRE(isValid);
}

BOOST_AUTO_TEST_CASE(auto_connect)
{
    addInstances(3);
    for (int i = 0; i < 100; ++i) {
        communications[0]->checkConnections();
        if (communications[0]->getActiveConnectionsCount() == 2 &&
            communications[1]->getActiveConnectionsCount() == 1 &&
            communications[2]->getActiveConnectionsCount() == 1)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    BOOST_TEST_REQUIRE(communications[0]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[0]->getConnectionByMinerId(blockchains[2]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[1]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[2]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    for (int i = 0; i < 100; ++i) {
        communications[1]->checkConnections();
        if (communications[0]->getActiveConnectionsCount() == 2 &&
            communications[1]->getActiveConnectionsCount() == 2 &&
            communications[2]->getActiveConnectionsCount() == 2)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    BOOST_TEST_REQUIRE(communications[1]->getConnectionByMinerId(blockchains[2]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[2]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    addInstances(2);
    for (int i = 0; i < 100; ++i) {
        communications[0]->checkConnections();
        communications[1]->checkConnections();
        if (communications[0]->getActiveConnectionsCount() == 4 &&
            communications[1]->getActiveConnectionsCount() == 4)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    BOOST_TEST_REQUIRE(communications[0]->getActiveConnectionsCount() == 4);
    BOOST_TEST_REQUIRE(communications[1]->getActiveConnectionsCount() == 4);
    BOOST_TEST_REQUIRE(communications[3]->getActiveConnectionsCount() == 2);
    BOOST_TEST_REQUIRE(communications[4]->getActiveConnectionsCount() == 2);
    communications[1].reset();
    for (int i = 0; i < 100; ++i) {
        if (communications[0]->getActiveConnectionsCount() == 3 &&
            communications[2]->getActiveConnectionsCount() == 1)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    BOOST_TEST_REQUIRE(!communications[0]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    BOOST_TEST_REQUIRE(!communications[2]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[0]->getActiveConnectionsCount() == 3);
    BOOST_TEST_REQUIRE(communications[3]->getActiveConnectionsCount() == 1);
    BOOST_TEST_REQUIRE(communications[4]->getActiveConnectionsCount() == 1);
}

BOOST_AUTO_TEST_CASE(auto_connect_many)
{
    addInstances(12);
    for (int i = 0; i < 500; ++i) {
        bool isValid = true;
        communications[i % communications.size()]->checkConnections();
        for (auto& communication : communications) {
            if (communication->getActiveConnectionsCount() < 5)
                isValid = false;
        }
        if (isValid)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    for (auto& communication : communications) {
        BOOST_TEST_REQUIRE(communication->getActiveConnectionsCount() >= 5);
    }
    for (auto& communication : communications) {
        communication->closeAllConnections();
    }
    std::this_thread::sleep_for(chrono::milliseconds(100));
    // fast open and close again, maybe it will generate some error
    for (auto& communication : communications) {
        communication->checkConnections();
        communication->closeAllConnections();
    }
    std::this_thread::sleep_for(chrono::milliseconds(100));

    for (int i = 0; i < 300; ++i) {
        bool isValid = true;
        for (auto& communication : communications) {
            if (communication->getActiveConnectionsCount() != 0)
                isValid = false;
        }
        if (isValid)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    for (auto& communication : communications) {
        BOOST_TEST_REQUIRE(communication->getActiveConnectionsCount() == 0);
    }

    bool isValid = true;
    for (int i = 0; i < 500; ++i) {
        isValid = true;
        communications[i % communications.size()]->checkConnections();
        for (auto& communication : communications) {
            if (communication->getActiveConnectionsCount() < 5)
                isValid = false;
        }
        if (isValid) {
            break;
        }
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    if (!isValid) {
        for (auto& communication : communications) {
            if (!communication)
                continue;
            BOOST_TEST_MESSAGE(communication->getDebugInfo());
        }
    }
    BOOST_TEST_REQUIRE(isValid);

    for (auto& communication : communications) {
        BOOST_TEST_REQUIRE(communication->getActiveConnectionsCount() >= 5);
    }

    // shut down 3 instances
    for (size_t i = 0; i < 3; ++i) {
        communications[i].reset();
    }

    for (int i = 0; i < 500; ++i) {
        isValid = true;
        if (communications[i % communications.size()]) {
            communications[i % communications.size()]->checkConnections();
        }
        for (auto& communication : communications) {
            if (!communication)
                continue;
            if (communication->getActiveConnectionsCount() < 5)
                isValid = false;
            for (size_t x = 0; x < 3; ++x) {
                if (auto connection = communication->getConnectionByMinerId(blockchains[x]->getMinerId())) {
                    isValid = isValid && !connection->isAccepted();
                }
            }
        }
        if (isValid)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }
    if (!isValid) {
        for (auto& communication : communications) {
            if (!communication)
                continue;
            BOOST_TEST_MESSAGE(communication->getDebugInfo());
        }
    }
    BOOST_TEST_REQUIRE(isValid);
}

BOOST_AUTO_TEST_CASE(send_block)
{
    addInstances(2);

    BOOST_TEST_REQUIRE(users[0].createUser(PublicKey::generateRandom()));

    // try to synchronize first block
    blockchains[0]->mineAndAddBlock();
    BOOST_TEST_REQUIRE(synchronizeInstances(30));

    // send next block, this time with 10 transactions
    auto keys = PrivateKey::generate(10);
    for (auto& key : keys) {
        BOOST_TEST_REQUIRE(users[0].createUser(key.publicKey()));
    }
    blockchains[0]->mineAndAddBlock();
    BOOST_TEST_REQUIRE(synchronizeInstances(30));
    BOOST_TEST_REQUIRE(databases[0]->confirmed().transactions.getTransactionsCount() ==
                       databases[1]->confirmed().transactions.getTransactionsCount());

    // close connection, mine 40 new blocks, skip two blocks and mine one more and establish connection again
    communications[1]->closeAllConnections();
    BOOST_TEST_REQUIRE(communications[1]->getActiveConnectionsCount() == 0);
    for (auto& communication : communications) {
        communication->clearBlockMinerIds();
    }

    for (size_t i = 0; i < 40; ++i) {
        blockchains[0]->mineAndAddBlock();
    }

    // skip 2 blocks and mine new block
    blockchains[0]->addBlock(blockchains[0]->mineBlock(2));
    blockchains[1]->setExpectedBlockId(blockchains[0]->getExpectedBlockId() + 1);
    BOOST_TEST_REQUIRE(synchronizeInstances(30));

    for (auto& key : PrivateKey::generate(2048)) {
        BOOST_TEST_REQUIRE(users[0].createUser(key.publicKey()));
    }

    blockchains[0]->mineAndAddBlock();
    BOOST_TEST_REQUIRE(synchronizeInstances(30));
    BOOST_TEST_REQUIRE(databases[0]->confirmed().transactions.getTransactionsCount() ==
                       databases[1]->confirmed().transactions.getTransactionsCount());
}

BOOST_AUTO_TEST_CASE(desynchronization_and_synchronization)
{
    addInstances(3);
    // skip kMinersQueueSize blocks to get mining queue with all instances
    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + kMinersQueueSize));
    BOOST_TEST_REQUIRE(synchronizeInstances(30));
    for (auto& communication : communications) {
        communication->closeAllConnections();
    }
    for (auto& communication : communications) {
        communication->clearBlockMinerIds();
    }
    // on every instance create maxiumum number of blocks with kDatabaseRolbackableBlocks limit
    uint32_t blockId = blockchains[0]->getLatestBlockId() + 1;
    uint32_t lastBlockId = blockId + kDatabaseRolbackableBlocks * blockchains.size();
    for (; blockId < lastBlockId; ++blockId) {
        BOOST_TEST_REQUIRE(mineAndAddBlock(blockId));
    }
    // make sure blockchains are desynchronized
    BOOST_TEST_REQUIRE(blockchains[0]->getLatestBlockId() != blockchains[1]->getLatestBlockId());
    BOOST_TEST_REQUIRE(blockchains[0]->getLatestBlockId() != blockchains[2]->getLatestBlockId());
    BOOST_TEST_REQUIRE(blockchains[1]->getLatestBlockId() != blockchains[2]->getLatestBlockId());
    // create one more block to have longest branch
    BOOST_TEST_REQUIRE(mineAndAddBlock(++blockId));
    // synchronize instances
    BOOST_TEST_REQUIRE(synchronizeInstances(60));
}

BOOST_AUTO_TEST_CASE(new_transactions)
{
    addInstances(1);

    // create prefix
    users[0].createUser(PublicKey::generateRandom());
    users[0].createPrefix("XXXX");

    addInstances(2);

    // desynchronize instances
    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::gamma_distribution<> distr(1024, 2);

    std::vector<Transaction_cptr> transactions;
    for (size_t transactionSize = 0; auto & key : PrivateKey::generate(2048)) {
        // small transaction
        uint32_t blockId = blockchains[0]->getLatestBlockId();
        transactions.push_back(
            CreateUserTransaction::create(blockId, -1, key.publicKey(), kUserMinFreeTransactions)->
            setUserId(users[0]->getId())->sign({ blockchains[0]->getMinerKey() })
        );
        transactionSize += transactions.back()->getSize();
        // big transaction
        std::string randomValue((size_t)(distr(gen)) % kStorageEntryMaxValueLength, 'X');
        std::string randomKey = Hash::generateRandom().toString();
        transactions.push_back(
            StorageAddEntryTransaction::create(blockId, -1, "XXXX", randomKey, randomValue)->
            setUserId(users[0]->getId())->sign({ blockchains[0]->getMinerKey() })
        );
        transactionSize += transactions.back()->getSize();
        if (transactionSize > kBlockMaxTransactionsSize * 2) {
            break;
        }
    }

    // post some transactions to second blockchain and some to third blockchain, have some duplicates
    auto transactions1 = std::vector<Transaction_cptr>(transactions.begin(),
                                                       transactions.begin() + (transactions.size() / 3 * 2));
    auto transactions2 = std::vector<Transaction_cptr>(transactions.begin() + (transactions.size() / 3 * 1),
                                                       transactions.end());
    BOOST_TEST_REQUIRE(blockchains[1]->postTransactions(transactions1));
    BOOST_TEST_REQUIRE(blockchains[2]->postTransactions(transactions2));

    BOOST_TEST_REQUIRE(synchronizeTransactions(30, { transactions }));

    for (auto& transaction : transactions) {
        BOOST_TEST_REQUIRE(databases[0]->unconfirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[1]->unconfirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[2]->unconfirmed().transactions.getTransaction(transaction->getId()));
    }

    // 3 blocks should be enough to fit all new transaction
    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
    BOOST_TEST_REQUIRE(synchronizeInstances(30));

    for (auto& transaction : transactions) {
        BOOST_TEST_REQUIRE(databases[0]->confirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[1]->confirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[2]->confirmed().transactions.getTransaction(transaction->getId()));
    }

    // post one more transaction
    std::string randomValue(kStorageEntryMaxValueLength, 'X');
    std::string randomKey = Hash::generateRandom().toString();
    auto transaction = StorageAddEntryTransaction::create(
        blockchains[1]->getExpectedBlockId(), -1, "XXXX", randomKey, randomValue
    )->setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });

    BOOST_TEST_REQUIRE(blockchains[1]->postTransaction(transaction));
    BOOST_TEST_REQUIRE(synchronizeTransactions(30, { transaction }));

    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
    BOOST_TEST_REQUIRE(synchronizeInstances(30));

    BOOST_TEST_REQUIRE(databases[0]->confirmed().transactions.getTransaction(transaction->getId()));
    BOOST_TEST_REQUIRE(databases[1]->confirmed().transactions.getTransaction(transaction->getId()));
    BOOST_TEST_REQUIRE(databases[2]->confirmed().transactions.getTransaction(transaction->getId()));
}

BOOST_AUTO_TEST_CASE(desynchronized_new_transactions)
{
    addInstances(3);

    // skip enough blocks to generate mining queue with every instance
    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + kMinersQueueSize));
    BOOST_TEST_REQUIRE(synchronizeInstances(30));

    // desynchronize instances, create few blocks with random transactions
    for (auto& communication : communications) {
        communication->closeAllConnections();
    }
    for (auto& communication : communications) {
        communication->clearBlockMinerIds();
    }

    // mine 8 blocks, each with one random transaction
    std::vector<Transaction_cptr> transactions;

    {
        uint32_t blockId = blockchains[0]->getLatestBlockId() + 1;
        for (uint32_t i = 0; i < 8; ++i) {
            auto key = PrivateKey::generate();
            auto transaction = CreateUserTransaction::create(
                blockId, -1, key.publicKey(), kUserMinFreeTransactions
            )->setUserId(blockchains[0]->getUserId())->sign({ blockchains[0]->getMinerKey() });
            BOOST_TEST_REQUIRE(mineAndAddBlock(blockId + i, { transaction }));
            transactions.push_back(transaction);
        }
        // mine one more block to have longest branch
        BOOST_TEST_REQUIRE(mineAndAddBlock(blockId + 9));
    }

    BOOST_TEST_REQUIRE(synchronizeInstances(30));
    BOOST_TEST_REQUIRE(synchronizeTransactions(30, transactions));

    for (auto& transaction : transactions) {
        BOOST_TEST_REQUIRE(databases[0]->unconfirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[1]->unconfirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[2]->unconfirmed().transactions.getTransaction(transaction->getId()));
    }

    BOOST_TEST_REQUIRE(mineAndAddBlock(blockchains[0]->getLatestBlockId() + 1));
    BOOST_TEST_REQUIRE(synchronizeInstances(30));

    for (auto& transaction : transactions) {
        BOOST_TEST_REQUIRE(databases[0]->confirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[1]->confirmed().transactions.getTransaction(transaction->getId()));
        BOOST_TEST_REQUIRE(databases[2]->confirmed().transactions.getTransaction(transaction->getId()));
    }
}

BOOST_AUTO_TEST_CASE(trusted_nodes)
{
    addInstances(1);
    BlockchainOptions blockchainOptions;
    CommunicationOptions communicationOptions = communications[0]->getOptions();
    communicationOptions.trustedNodes.emplace(blockchains[0]->getMinerId(),
                                              Endpoint(communicationOptions.host, communicationOptions.port));
    addInstances(2, blockchainOptions, communicationOptions, false);
    for (int i = 0; i < 100; ++i) {
        communications[1]->checkConnections();
        communications[2]->checkConnections();
        if (communications[0]->getActiveConnectionsCount() == 2 &&
            communications[1]->getActiveConnectionsCount() == 1 &&
            communications[2]->getActiveConnectionsCount() == 1)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }

    BOOST_TEST_REQUIRE(communications[0]->getConnectionByMinerId(blockchains[1]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[0]->getConnectionByMinerId(blockchains[2]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[1]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    BOOST_TEST_REQUIRE(communications[2]->getConnectionByMinerId(blockchains[0]->getMinerId()));
    for (int i = 0; i < 100; ++i) {
        blockchains[1]->update();
        blockchains[2]->update();
        if (databases[0]->confirmed().blocks.getLatestBlockId() == databases[1]->confirmed().blocks.getLatestBlockId() &&
            databases[0]->confirmed().blocks.getLatestBlockId() == databases[2]->confirmed().blocks.getLatestBlockId())
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }

    BOOST_TEST_REQUIRE(databases[0]->confirmed().blocks.getLatestBlockId() == databases[1]->confirmed().blocks.getLatestBlockId());
    BOOST_TEST_REQUIRE(databases[0]->confirmed().blocks.getLatestBlockId() == databases[2]->confirmed().blocks.getLatestBlockId());

    BOOST_TEST_REQUIRE(databases[1]->confirmed().miners.getMiner(blockchains[2]->getMinerId())->settings.endpoint.isValid());
    BOOST_TEST_REQUIRE(databases[2]->confirmed().miners.getMiner(blockchains[1]->getMinerId())->settings.endpoint.isValid());

    for (int i = 0; i < 300; ++i) {
        communications[1 + i % 2]->checkConnections();
        communications[1 + i % 2]->checkConnections();
        if (communications[0]->getActiveConnectionsCount() == 2 &&
            communications[1]->getActiveConnectionsCount() == 2 &&
            communications[2]->getActiveConnectionsCount() == 2)
            break;
        std::this_thread::sleep_for(chrono::milliseconds(100));
    }

    BOOST_TEST_REQUIRE(communications[0]->getActiveConnectionsCount() == 2);
    BOOST_TEST_REQUIRE(communications[1]->getActiveConnectionsCount() == 2);
    BOOST_TEST_REQUIRE(communications[2]->getActiveConnectionsCount() == 2);
}

BOOST_AUTO_TEST_SUITE_END();
