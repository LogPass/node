#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/block/block.h>
#include <blockchain/block/pending_block.h>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/storage/create_prefix.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(pending_block);

BOOST_AUTO_TEST_CASE(general)
{
    auto key = PrivateKey::generate();

    std::vector<Transaction_cptr> transactions = {
        CreateUserTransaction::create(1, -1, key.publicKey(), 4)->setUserId(key.publicKey())->sign({ key }),
        CreateUserTransaction::create(2, -1, key.publicKey(), 4)->setUserId(key.publicKey())->sign({ key }),
        StorageCreatePrefixTransaction::create(1, -1, "XXXXXXXX")->setUserId(key.publicKey())->sign({ key }),
    };
    auto otherTransaction = StorageCreatePrefixTransaction::create(2, -1, "XXXXXXXX")->
        setPublicKey(key.publicKey())->setUserId(key.publicKey())->sign({ key });

    MinersQueue nextMiners = { MinerId(key.publicKey()) };
    auto firstBlocks = Block::create(1, 1, nextMiners, { transactions[0], otherTransaction }, Hash(), key);
    auto block = Block::create(2, 2, nextMiners, transactions, firstBlocks->getHeaderHash(), key);

    size_t callbackCalls = 0;
    PendingBlock_ptr pendingBlock = std::make_shared<PendingBlock>(block->getBlockHeader(), key.publicKey(),
                                                       [&](PendingBlock_ptr pendingBlock2) {
        BOOST_REQUIRE(pendingBlock == pendingBlock2);
        callbackCalls += 1;
    });
    BOOST_TEST_REQUIRE(callbackCalls == 0);
    BOOST_TEST_REQUIRE(pendingBlock->getStatus() == PendingBlock::Status::MISSING_BODY);
    BOOST_TEST_REQUIRE(pendingBlock->getBlockBodyHash() == block->getBodyHash());

    // invalid block body
    BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(firstBlocks->getBlockBody()) == PendingBlock::AddResult::INVALID_DATA);
    BOOST_TEST_REQUIRE(callbackCalls == 0);

    // valid block body
    BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
    BOOST_TEST_REQUIRE(callbackCalls == 1);

    // duplicated block body
    BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::DUPLICATED);
    BOOST_TEST_REQUIRE(callbackCalls == 1);
    BOOST_TEST_REQUIRE(pendingBlock->getStatus() == PendingBlock::Status::MISSING_TRANSACTION_IDS);

    // once more invalid block body
    BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(firstBlocks->getBlockBody()) == PendingBlock::AddResult::INVALID_DATA);
    BOOST_TEST_REQUIRE(callbackCalls == 1);

    auto firstBlocksTransactionIds = firstBlocks->getBlockTransactionIds();
    auto blockTransactionIds = block->getBlockTransactionIds();
    BOOST_TEST_REQUIRE(firstBlocksTransactionIds.size() == 1);
    BOOST_TEST_REQUIRE(blockTransactionIds.size() == 1);

    // make sure missing transaction ids hashes are correct
    auto missingTransactionIdsHashes = pendingBlock->getMissingTransactionIdsHashes();
    BOOST_TEST_REQUIRE(missingTransactionIdsHashes.size() == 1);
    BOOST_TEST_REQUIRE(missingTransactionIdsHashes[0].first == 0);
    BOOST_TEST_REQUIRE(missingTransactionIdsHashes[0].second == blockTransactionIds[0]->getHash());

    // invalid transaction ids
    BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(firstBlocksTransactionIds[0]) ==
                       PendingBlock::AddResult::INVALID_DATA);
    BOOST_TEST_REQUIRE(callbackCalls == 1);
    BOOST_TEST_REQUIRE(pendingBlock->getMissingTransactionIds().size() == 0);
    BOOST_TEST_REQUIRE(pendingBlock->getStatus() == PendingBlock::Status::MISSING_TRANSACTION_IDS);

    // valid transaction ids
    BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds[0]) ==
                       PendingBlock::AddResult::CORRECT);
    BOOST_TEST_REQUIRE(callbackCalls == 2);

    // make sure missing transaction ids hashes are still correct
    missingTransactionIdsHashes = pendingBlock->getMissingTransactionIdsHashes();
    BOOST_TEST_REQUIRE(missingTransactionIdsHashes.size() == 0);

    // duplicated transaction ids
    BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds[0]) ==
                       PendingBlock::AddResult::DUPLICATED);
    BOOST_TEST_REQUIRE(callbackCalls == 2);

    // add invalid transaction
    BOOST_TEST_REQUIRE(pendingBlock->addTransaction(otherTransaction) == PendingBlock::AddResult::INVALID_DATA);
    BOOST_TEST_REQUIRE(callbackCalls == 2);
    BOOST_TEST_REQUIRE(pendingBlock->getMissingTransactionIds().size() == 3);

    // add one correct transaction
    BOOST_TEST_REQUIRE(pendingBlock->addTransaction(transactions[0]) == PendingBlock::AddResult::CORRECT);
    BOOST_TEST_REQUIRE(callbackCalls == 3);
    BOOST_TEST_REQUIRE(pendingBlock->getMissingTransactionIds().size() == 2);

    // add duplicated transaction
    BOOST_TEST_REQUIRE(pendingBlock->addTransaction(transactions[0]) == PendingBlock::AddResult::DUPLICATED);
    BOOST_TEST_REQUIRE(callbackCalls == 3);

    // add other correct transaction without executing callback
    BOOST_TEST_REQUIRE(pendingBlock->addTransaction(transactions[1], false) == PendingBlock::AddResult::CORRECT);
    BOOST_TEST_REQUIRE(callbackCalls == 3);
    BOOST_TEST_REQUIRE(pendingBlock->getMissingTransactionIds().size() == 1);

    // add last correct transaction
    BOOST_TEST_REQUIRE(pendingBlock->addTransaction(transactions[2]) == PendingBlock::AddResult::CORRECT);
    BOOST_TEST_REQUIRE(callbackCalls == 4);
    BOOST_TEST_REQUIRE(pendingBlock->getMissingTransactionIds().size() == 0);

    // once more add invalid transaction
    BOOST_TEST_REQUIRE(pendingBlock->addTransaction(otherTransaction) == PendingBlock::AddResult::INVALID_DATA);
    BOOST_TEST_REQUIRE(callbackCalls == 4);

    // check is transactions are listed correctly
    auto pendingBlockTransactionIds = pendingBlock->getTransactionIds();
    BOOST_TEST_REQUIRE(pendingBlockTransactionIds.size() == 3);
    BOOST_TEST_REQUIRE(pendingBlockTransactionIds.count(transactions[0]->getId()) == 1);
    BOOST_TEST_REQUIRE(pendingBlockTransactionIds.count(transactions[1]->getId()) == 1);
    BOOST_TEST_REQUIRE(pendingBlockTransactionIds.count(transactions[2]->getId()) == 1);

    // create block
    BOOST_TEST_REQUIRE(!pendingBlock->isInvalid());
    BOOST_TEST_REQUIRE(pendingBlock->getStatus() == PendingBlock::Status::COMPLETE);
    auto newBlock = pendingBlock->createBlock();
    BOOST_TEST_REQUIRE(newBlock);
    BOOST_TEST_REQUIRE(newBlock->getHeaderHash() == block->getHeaderHash());
    BOOST_TEST_REQUIRE(newBlock->getBodyHash() == block->getBodyHash());
    BOOST_TEST_REQUIRE(callbackCalls == 4);

    // set pending block as invalid
    pendingBlock->setInvalid();
    BOOST_TEST_REQUIRE(pendingBlock->isInvalid());
    BOOST_TEST_REQUIRE(callbackCalls == 4);

    // try to set invalid one more time
    pendingBlock->setInvalid();
    BOOST_TEST_REQUIRE(callbackCalls == 4);
}

BOOST_AUTO_TEST_SUITE_END();
