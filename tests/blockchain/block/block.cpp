#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/block/block.h>
#include <blockchain/transactions/create_user.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(block);

BOOST_AUTO_TEST_CASE(serialization)
{
    auto key = PrivateKey::generate();
    uint32_t blockId = 10;
    MinersQueue nextMiners = { key.publicKey(), PrivateKey::generate().publicKey() };
    std::vector<Transaction_cptr> transactions;
    for (int i = 0; i < 1050; ++i) {
        auto createUserTransaction = CreateUserTransaction::create(blockId, -1, PublicKey::generateRandom(), 4)->
            setUserId(key.publicKey())->sign({ key });
        transactions.push_back(createUserTransaction);
    }
    Hash prevBlockHash = Hash::generate("X");
    auto block = Block::create(blockId, 8, nextMiners, transactions, prevBlockHash, key);
    BOOST_TEST_REQUIRE(block->getId() == blockId);
    BOOST_TEST_REQUIRE(block->getDepth() == 8);
    BOOST_TEST_REQUIRE(block->validate(key.publicKey(), prevBlockHash));
    BOOST_TEST_REQUIRE(!block->validate(key.publicKey(), Hash::generate("Y")));
    BOOST_TEST_REQUIRE(!block->validate(PrivateKey::generate().publicKey(), prevBlockHash));
    Serializer s;
    s(block);
    s.switchToReader();
    auto block2 = std::make_shared<Block>();
    s(block2);
    BOOST_TEST_REQUIRE(block2->validate(key.publicKey(), prevBlockHash));
    BOOST_TEST_REQUIRE(!block2->validate(key.publicKey(), Hash::generate("Y")));
    BOOST_TEST_REQUIRE(!block2->validate(PrivateKey::generate().publicKey(), prevBlockHash));
    BOOST_TEST_REQUIRE(block->getNextMiners() == nextMiners);
    BOOST_TEST_REQUIRE(block->getId() == block2->getId());
    BOOST_TEST_REQUIRE(block->getDepth() == block2->getDepth());
    BOOST_TEST_REQUIRE(block->getHeaderHash() == block2->getHeaderHash());
    BOOST_TEST_REQUIRE(block->getNextMiners() == block2->getNextMiners());
    BOOST_TEST_REQUIRE(block->getTransactions() == block2->getTransactions());
    BOOST_TEST_REQUIRE(block->getBlockBody()->getTransactions() == block2->getBlockBody()->getTransactions());
    for (size_t i = 0; i < block->getTransactions(); ++i) {
        BOOST_TEST_REQUIRE(block->getTransactionId(i) == block2->getTransactionId(i));
        BOOST_TEST_REQUIRE(block->getTransaction(block->getTransactionId(i)) == block->getTransaction(block2->getTransactionId(i)));
        BOOST_TEST_REQUIRE(block2->getTransaction(block2->getTransactionId(i))->getId() == block->getTransactionId(i));
    }

    std::stringstream stream;
    Serializer s2;
    s2(block2);
    s2.writeTo(stream);
    stream.seekg(0, stream.beg);
    Serializer s3(stream);
    auto block3 = std::make_shared<Block>();
    s3(block3);
    BOOST_TEST_REQUIRE(block3->validate(key.publicKey(), prevBlockHash));
    BOOST_TEST_REQUIRE(!block3->validate(key.publicKey(), Hash::generate("Y")));
    BOOST_TEST_REQUIRE(!block3->validate(PrivateKey::generate().publicKey(), prevBlockHash));
    BOOST_TEST_REQUIRE(block->getNextMiners() == nextMiners);
    BOOST_TEST_REQUIRE(block->getId() == block3->getId());
    BOOST_TEST_REQUIRE(block->getHeaderHash() == block3->getHeaderHash());
    BOOST_TEST_REQUIRE(block->getNextMiners() == block3->getNextMiners());
    BOOST_TEST_REQUIRE(block->getTransactions() == block3->getTransactions());
    BOOST_TEST_REQUIRE(block->getBlockBody()->getTransactions() == block3->getBlockBody()->getTransactions());
    for (size_t i = 0; i < block->getTransactions(); ++i) {
        BOOST_TEST_REQUIRE(block->getTransactionId(i) == block3->getTransactionId(i));
        BOOST_TEST_REQUIRE(block->getTransaction(block->getTransactionId(i)) == block->getTransaction(block3->getTransactionId(i)));
        BOOST_TEST_REQUIRE(block3->getTransaction(block3->getTransactionId(i))->getId() == block->getTransactionId(i));
    }
}

BOOST_AUTO_TEST_SUITE_END();
