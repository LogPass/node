#include "pch.h"

#include <boost/test/unit_test.hpp>

#include "database_fixture.h"

using namespace logpass;

BOOST_FIXTURE_TEST_SUITE(database, DatabaseFixture<>);

BOOST_AUTO_TEST_CASE(initialization_and_rollback)
{
    auto key = PrivateKey::generate();
    MinersQueue nextMiners = { MinerId(key.publicKey()) };
    auto block1 = Block::create(1, 1, nextMiners, {}, Hash(), key);
    auto block2 = Block::create(2, 2, nextMiners, {}, block1->getHeaderHash(), key);

    db->unconfirmed().blocks.addBlock(block1);
    db->commit(1);
    db->unconfirmed().blocks.addBlock(block2);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getBlockHeader(1) != nullptr);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getBlockHeader(2) == nullptr);
    BOOST_TEST_REQUIRE(db->unconfirmed().blocks.getBlockHeader(2) != nullptr);
    db->clear();
    BOOST_TEST_REQUIRE(db->unconfirmed().blocks.getBlockHeader(2) == nullptr);
    db->unconfirmed().blocks.addBlock(block2);
    BOOST_TEST_REQUIRE(db->unconfirmed().blocks.getBlockHeader(2) != nullptr);
    db->commit(2);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getLatestBlocks().size() == 2);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getBlockHeader(2) != nullptr);
    BOOST_TEST_REQUIRE(db->getMaxRollbackDepth() == 2);
    db->rollback(1);
    BOOST_TEST_REQUIRE(db->getMaxRollbackDepth() == 1);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getBlockHeader(2) == nullptr);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getBlockHeader(1) != nullptr);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getLatestBlocks().size() == 1);
    reinitialize();
    BOOST_TEST_REQUIRE(db->unconfirmed().blocks.getBlockHeader(1) != nullptr);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getBlockHeader(1) != nullptr);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getLatestBlocks().size() == 1);
    BOOST_TEST_REQUIRE(db->getMaxRollbackDepth() == 1);
}

BOOST_AUTO_TEST_CASE(max_rollback)
{
    auto key = PrivateKey::generate();
    Block_cptr block;
    for (size_t i = 1; i <= kDatabaseRolbackableBlocks * 2; ++i) {
        MinersQueue nextMiners = { MinerId(key.publicKey()) };
        block = Block::create(i, i, nextMiners, {}, block ? block->getHeaderHash() : Hash(), key);
        db->unconfirmed().blocks.addBlock(block);
        db->commit(i);
    }
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getLatestBlocks().size() == kDatabaseRolbackableBlocks * 2);
    BOOST_TEST_REQUIRE(db->getMaxRollbackDepth() == kDatabaseRolbackableBlocks);
    BOOST_TEST_REQUIRE(db->rollback(kDatabaseRolbackableBlocks / 2));
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getLatestBlocks().size() == kDatabaseRolbackableBlocks * 3 / 2);
    reinitialize();
    BOOST_TEST_REQUIRE(db->getMaxRollbackDepth() == kDatabaseRolbackableBlocks / 2);
    BOOST_TEST_REQUIRE(db->rollback(kDatabaseRolbackableBlocks / 2));
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getLatestBlocks().size() == kDatabaseRolbackableBlocks);
    BOOST_TEST_REQUIRE(db->getMaxRollbackDepth() == 0);
    BOOST_TEST_REQUIRE(!db->rollback(1));
}

BOOST_AUTO_TEST_SUITE_END();
