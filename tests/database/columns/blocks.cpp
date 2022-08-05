#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/storage/create_prefix.h>
#include <database/columns/blocks.h>
#include <database/database_fixture.h>

using namespace logpass;
using namespace logpass::database;

class BlocksColumnDatabase : public BaseDatabase {
    friend class SharedThread<BlocksColumnDatabase>;
public:
    using BaseDatabase::BaseDatabase;

    void start() override
    {
        BaseDatabase::start({
            { BlocksColumn::getName(), BlocksColumn::getOptions() }
                            });
        blocks = std::make_unique<BlocksColumn>(m_db, m_handles[0]);
        load();
    }

    void stop() override
    {
        BaseDatabase::stop();
    }

    std::vector<Column*> columns() const override
    {
        return {
            blocks.get()
        };
    }

    std::unique_ptr<BlocksColumn> blocks;
};

BOOST_AUTO_TEST_SUITE(columns);
BOOST_FIXTURE_TEST_SUITE(blocks, DatabaseFixture<BlocksColumnDatabase>);

BOOST_AUTO_TEST_CASE(basic)
{
    auto key = PrivateKey::generate();
    std::vector<Transaction_cptr> transactions = {
        CreateUserTransaction::create(key.publicKey(), 1)->setUserId(key.publicKey())->setPublicKey(key.publicKey())->sign({ key }),
        StorageCreatePrefixTransaction::create(1, -1, "XXXXXXXX")->setUserId(key.publicKey())->setPublicKey(key.publicKey())->sign({ key })
    };
    MinersQueue nextMiners = { MinerId(key.publicKey()) };
    auto block1 = Block::create(1, 1, nextMiners, transactions, Hash(), key);
    auto block2 = Block::create(2, 2, nextMiners, transactions, block1->getHeaderHash(), key);

    db->blocks->addBlock(block1);
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(1, true) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockBody(1, true) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(1, 0, true) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(1, 1, true) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(1, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockBody(1, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(1, 0, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(true).size() == 0);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(false).size() == 1);
    BOOST_TEST_REQUIRE(db->blocks->getBlockId() == 0);
    db->commit(1);
    BOOST_TEST_REQUIRE(db->blocks->getBlockId() == 1);
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(1, true) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockBody(1, true) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(1, 0, true) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(1, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockBody(1, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(1, 0, false) != nullptr);
    db->blocks->addBlock(block2);
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(2, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockBody(2, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(2, 0, false) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(true).size() == 1);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(false).size() == 2);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlockHeader(true)->getId() == 1);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlockHeader(false)->getId() == 2);
    db->clear();
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(2, false) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockBody(2, false) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(2, 0, false) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(true).size() == 1);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(false).size() == 1);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlockHeader(false)->getId() == 1);
    db->blocks->addBlock(block2);
    db->commit(2);
    BOOST_TEST_REQUIRE(db->blocks->getBlockId() == 2);
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(2, true) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockHeader(2, true)->getId() == 2);
    BOOST_TEST_REQUIRE(db->blocks->getBlockBody(2, true) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getBlockTransactionIds(2, 0, true) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(true).size() == 2);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlocks(false).size() == 2);
    BOOST_TEST_REQUIRE(db->blocks->getLatestBlockHeader(true)->getId() == 2);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
