#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/storage/create_prefix.h>
#include <database/facades/blocks.h>
#include <database/database_fixture.h>

using namespace logpass;
using namespace logpass::database;

class BlocksFacadeDatabase : public BaseDatabase {
    friend class SharedThread<BlocksFacadeDatabase>;
public:
    using BaseDatabase::BaseDatabase;

    void start() override
    {
        BaseDatabase::start({
            { BlocksColumn::getName(), BlocksColumn::getOptions() },
            { TransactionsColumn::getName(), TransactionsColumn::getOptions() }
                            });
        blocksColumn = std::make_unique<BlocksColumn>(m_db, m_handles[0]);
        transactionsColumn = std::make_unique<TransactionsColumn>(m_db, m_handles[1]);
        load();
    }

    void stop() override
    {
        BaseDatabase::stop();
    }

    BlocksFacade blocks(bool confirmed = false)
    {
        return BlocksFacade(blocksColumn, transactionsColumn, confirmed);
    }

    std::vector<Column*> columns() const override
    {
        return {
            blocksColumn.get(), transactionsColumn.get()
        };
    }

    std::unique_ptr<BlocksColumn> blocksColumn;
    std::unique_ptr<TransactionsColumn> transactionsColumn;
};

BOOST_AUTO_TEST_SUITE(facades);
BOOST_FIXTURE_TEST_SUITE(blocks, DatabaseFixture<BlocksFacadeDatabase>);

BOOST_AUTO_TEST_CASE(basic)
{
    auto key = PrivateKey::generate();
    std::vector<Transaction_cptr> transactions = {
        CreateUserTransaction::create(key.publicKey(), 1)->setUserId(key.publicKey())->setPublicKey(key.publicKey())->sign({ key }),
        StorageCreatePrefixTransaction::create(1, -1, "XXXXXXXX")->setUserId(key.publicKey())->setPublicKey(key.publicKey())->sign({ key })
    };
    MinersQueue nextMiners = { MinerId(key.publicKey()) };
    auto block1 = Block::create(1, 1, nextMiners, transactions, Hash(), key);
    auto block2 = Block::create(2, 2, nextMiners, {}, block1->getHeaderHash(), key);

    db->transactionsColumn->addTransaction(transactions[0], 1);
    db->transactionsColumn->addTransaction(transactions[1], 1);
    db->blocks().addBlock(block1);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(1) != nullptr);
    db->commit(1);
    db->blocks().addBlock(block2);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(2) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(2)->getId() == 2);
    BOOST_TEST_REQUIRE(db->blocks(true).getBlock(2) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(1) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(1)->getId() == 2);
    BOOST_TEST_REQUIRE(db->blocks(true).getNextBlockHeader(1) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(2) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getLatestBlocks().size() == 2);
    BOOST_TEST_REQUIRE(db->blocks(true).getLatestBlocks().size() == 1);
    db->clear();
    BOOST_TEST_REQUIRE(db->blocks().getBlock(2) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(1) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getLatestBlocks().size() == 1);
    db->blocks().addBlock(block2);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(2) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(2)->getId() == 2);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(1) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(1)->getId() == 2);
    db->commit(2);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(1) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(1)->getId() == 1);
    BOOST_TEST_REQUIRE(db->blocks().getBlockHeader(1)->getHash() == block1->getHeaderHash());
    BOOST_TEST_REQUIRE(db->blocks().getBlockBody(1)->getHash() == block1->getBodyHash());
    BOOST_TEST_REQUIRE(db->blocks().getBlockTransactionIds(1, 0)->at(0) == transactions[0]->getId());
    BOOST_TEST_REQUIRE(db->blocks().getBlockTransactionIds(1, 0)->at(1) == transactions[1]->getId());
    BOOST_TEST_REQUIRE(db->blocks().getBlockTransactionIds(1, 1) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(2) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(2)->getId() == 2);
    BOOST_TEST_REQUIRE(db->blocks().getBlock(3) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(1) != nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(1)->getId() == 2);
    BOOST_TEST_REQUIRE(db->blocks().getNextBlockHeader(2) == nullptr);
    BOOST_TEST_REQUIRE(db->blocks().getLatestBlocks().size() == 2);
    BOOST_TEST_REQUIRE(db->blocks(true).getLatestBlocks().size() == 2);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
