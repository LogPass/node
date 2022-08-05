#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/lock_user.h>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/create_miner.h>
#include <blockchain/transactions/increase_stake.h>
#include <blockchain/transactions/commit.h>
#include <blockchain/transactions/transfer.h>
#include <blockchain/transactions/update_user.h>
#include <blockchain/transactions/select_miner.h>
#include <blockchain/transactions/storage/add_entry.h>
#include <blockchain/transactions/storage/create_prefix.h>
#include <blockchain/transactions/storage/update_prefix.h>
#include "blockchain_fixture.h"

using namespace logpass;

struct EmptyFixture {};

BOOST_FIXTURE_TEST_SUITE(blockchain, BlockchainFixture);

BOOST_AUTO_TEST_CASE(initialization)
{
    // basic initialization
    auto user = db->confirmed().users.getUser(blockchain->getUserId());
    auto miner = db->confirmed().miners.getMiner(blockchain->getMinerId());
    BOOST_TEST_REQUIRE(user);
    BOOST_TEST_REQUIRE(user->tokens == kFirstUserBalance);
    BOOST_TEST_REQUIRE(user->creator == blockchain->getUserId());
    BOOST_TEST_REQUIRE(miner);
    BOOST_TEST_REQUIRE(miner->stake == kFirstUserStake);
    BOOST_TEST_REQUIRE(miner->owner == blockchain->getUserId());
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getLatestBlockId() == 1);
    BOOST_TEST_REQUIRE(db->confirmed().users.getTokens() == kFirstUserBalance);
    BOOST_TEST_REQUIRE(db->confirmed().miners.getStakedTokens() == kFirstUserStake);
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getMinersQueue().size() == kMinersQueueSize);
}

BOOST_AUTO_TEST_CASE(skip_blocks)
{
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == 2);
    auto block = blockchain->mineBlock(100); // skips 100 blocks
    BOOST_TEST_REQUIRE(block->getNextMiners().size() == 101);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == 103);
    for (size_t i = 0; i < 40; ++i) {
        auto block = blockchain->mineBlock(kMinersQueueSize - 1);
        BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    }
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == 103 + 40 * kMinersQueueSize);
}

BOOST_AUTO_TEST_CASE(add_new_miner)
{
    TestUser user2 = user.createUser();
    BOOST_TEST_REQUIRE(user.transferTokens(user2, kFirstUserStake * 2));
    MinerId newMinerId(PublicKey::generateRandom());
    BOOST_TEST_REQUIRE(newMinerId != blockchain->getMinerId());
    BOOST_TEST_REQUIRE(user2.createMiner(newMinerId));
    // make sure miner stake change is not limited by miner history during first kMinersQueueSize blocks
    BOOST_TEST_REQUIRE(user2.selectMiner(newMinerId));
    BOOST_TEST_REQUIRE(user2.increaseStake(newMinerId, kFirstUserStake));
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    uint64_t preWithdrawTokens = user2->tokens;
    BOOST_TEST_REQUIRE(user2.withdrawStake(newMinerId, 0, kFirstUserStake));
    BOOST_TEST_REQUIRE(user2->tokens == preWithdrawTokens + kFirstUserStake * 19 / 20 - kTransactionFee);
    BOOST_TEST_REQUIRE(db->unconfirmed().miners.getMiner(newMinerId)->stake == 0);
    auto block = blockchain->mineBlock();
    BOOST_TEST_REQUIRE(block->getNextMiners()[0] == newMinerId);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    BOOST_TEST_REQUIRE(db->confirmed().miners.getMiner(newMinerId)->stake == 0);
    block = blockchain->mineBlock(kMinersQueueSize - 2);
    BOOST_TEST_REQUIRE(block->getNextMiners()[0] != newMinerId);
    BOOST_TEST_REQUIRE(block->getNextMiners()[1] != newMinerId);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getMinersQueue()[0] == newMinerId);
    block = blockchain->mineBlock(1);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    BOOST_TEST_REQUIRE(user2.increaseStake(newMinerId, kFirstUserStake));
    BOOST_TEST_REQUIRE(db->confirmed().blocks.getMinersQueue()[0] != newMinerId);
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    block = blockchain->mineBlock();
    BOOST_TEST_REQUIRE(block->getNextMiners()[0] == newMinerId);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    block = blockchain->mineBlock(kMinersQueueSize - 2);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    auto nextMiners = db->confirmed().blocks.getMinersQueue();
    size_t newMinerCount = std::count(nextMiners.begin(), nextMiners.end(), newMinerId);
    BOOST_TEST_REQUIRE(newMinerCount == kMinersQueueSize * 2 / 5);
}

BOOST_AUTO_TEST_CASE(post_transaction)
{
    Transaction_cptr transaction =
        CreateUserTransaction::create(PrivateKey::generate().publicKey(), blockchain->getLatestBlockId())->
        setUserId(blockchain->getUserId())->sign({ blockchain->getMinerKey() });
    Serializer s;
    s(transaction);
    s.buffer()[s.size() - 1] += 1;
    s.switchToReader();
    auto invalidTransaction = Transaction::load(s);
    BOOST_TEST_REQUIRE(!blockchain->postTransaction(invalidTransaction));
    BOOST_TEST_REQUIRE(blockchain->postTransaction(transaction));
}

BOOST_AUTO_TEST_CASE(rebuild_pending_transactions)
{
    auto block = blockchain->mineBlock();

    auto keys = PrivateKey::generate(100);
    for (auto& key : keys) {
        BOOST_TEST_REQUIRE(user.createUser(key.publicKey()));
        BOOST_TEST_REQUIRE(db->unconfirmed().users.getUser(key.publicKey()));
        BOOST_TEST_REQUIRE(!db->confirmed().users.getUser(key.publicKey()));
    }

    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    for (auto& key : keys) {
        BOOST_TEST_REQUIRE(!db->confirmed().users.getUser(key.publicKey()));
    }

    blockchain->update();
    for (auto& key : keys) {
        BOOST_TEST_REQUIRE(!db->confirmed().users.getUser(key.publicKey()));
        BOOST_TEST_REQUIRE(db->unconfirmed().users.getUser(key.publicKey()));
    }

    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    for (auto& key : keys) {
        auto user = db->confirmed().users.getUser(key.publicKey());
        BOOST_TEST_REQUIRE(user);
    }
}


BOOST_AUTO_TEST_CASE(recover_transactions)
{
    // ensure that when branch changes, transaction from old branch are added to pending transactions
    auto block2 = blockchain->mineBlock();
    auto block3 = Block::create(block2->getId() + 1, block2->getDepth() + 1, block2->getNextMiners(), {},
                                block2->getHeaderHash(), blockchain->getMinerKey());

    auto keys = PrivateKey::generate(100);
    for (auto& key : keys) {
        BOOST_TEST_REQUIRE(user.createUser(key.publicKey()));
        BOOST_TEST_REQUIRE(db->unconfirmed().users.getUser(key.publicKey()));
        BOOST_TEST_REQUIRE(!db->confirmed().users.getUser(key.publicKey()));
    }

    auto block4 = blockchain->mineBlock(3);
    BOOST_TEST_REQUIRE(block4->getTransactions() >= keys.size());
    BOOST_TEST_REQUIRE(blockchain->addBlock(block4));
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == block4->getId());

    BOOST_TEST_REQUIRE(blockchain->addBlock(block2, false));
    BOOST_TEST_REQUIRE(blockchain->addBlock(block3));
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == block3->getId());
    auto block5 = blockchain->mineBlock();
    BOOST_TEST_REQUIRE(block5->getTransactions() >= keys.size());
    BOOST_TEST_REQUIRE(blockchain->addBlock(block5));

    for (auto& key : keys) {
        BOOST_TEST_REQUIRE(db->confirmed().users.getUser(key.publicKey()));
    }
}


BOOST_AUTO_TEST_CASE(skip_and_rebuild_pending_transactions)
{
    auto block = blockchain->mineBlock(kTransactionMaxBlockIdDifference - 2);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == kTransactionMaxBlockIdDifference);

    block = blockchain->mineBlock(kTransactionMaxBlockIdDifference / 2 - 1);

    auto keys = PrivateKey::generate(kTransactionMaxBlockIdDifference);
    for (size_t blockId = 1; auto & key : keys) {
        user.setBlockId(++blockId);
        BOOST_TEST_REQUIRE(user.createUser(key.publicKey()));
        BOOST_TEST_REQUIRE(db->unconfirmed().users.getUser(key.publicKey()));
        BOOST_TEST_REQUIRE(!db->confirmed().users.getUser(key.publicKey()));
    }
    user.setBlockId(0);

    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == 3 * (kTransactionMaxBlockIdDifference / 2));
    BOOST_TEST_REQUIRE(blockchain->getExpectedBlockId() == blockchain->getLatestBlockId() + 1);
    blockchain->update();
    size_t validUsers = 0;
    for (auto& key : keys) {
        if (db->unconfirmed().users.getUser(key.publicKey())) {
            validUsers += 1;
        }
    }
    BOOST_TEST_REQUIRE(validUsers == kTransactionMaxBlockIdDifference / 2);

    block = blockchain->mineBlock(kTransactionMaxBlockIdDifference / 4);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    BOOST_TEST_REQUIRE(blockchain->getLatestBlockId() == 1 + 7 * (kTransactionMaxBlockIdDifference / 4));
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    blockchain->update();
    validUsers = 0;
    for (auto& key : keys) {
        if (db->confirmed().users.getUser(key.publicKey())) {
            validUsers += 1;
        }
    }
    BOOST_TEST_REQUIRE(validUsers == kTransactionMaxBlockIdDifference / 4);
}

BOOST_AUTO_TEST_CASE(create_and_update_users)
{
    auto users = user.createUsers(10);
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());

    auto keys = PrivateKey::generate(5);
    std::map<PublicKey, uint8_t> newKeys = {
        { keys[0].publicKey(), 5 },
        { keys[1].publicKey(), 10 },
        { keys[2].publicKey(), 15 },
        { keys[3].publicKey(), 20 },
        { keys[4].publicKey(), 25 }
    };

    UserSettings newSettings;
    newSettings.keys = UserKeys::create(newKeys);
    newSettings.rules.powerLevels = { 10, 15, 20, 25, 30 };
    newSettings.rules.keysUpdateTimes = { 50, 40, 30, 20, 10 };
    newSettings.rules.rulesUpdateTimes = { 50, 40, 30, 20, 10 };
    newSettings.rules.supervisorsUpdateTimes = { 50, 40, 30, 20, 10 };

    for (auto& user2 : users) {
        user2.setPricing(0);
        BOOST_TEST_REQUIRE(user2.updateUser(newSettings));
    }

    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    for (bool sponsored = false; auto & user : users) {
        BOOST_REQUIRE(user.confirmed()->settings == newSettings);
    }

    // update again
    newSettings.rules.supervisingPowerLevel = 1;
    for (auto& user : users) {
        user.setKeys({ keys[0] });
        // should fail, due to not enough power
        BOOST_TEST_REQUIRE(!user.updateUser(newSettings));
        user.setKeys({ keys[0], keys[1], keys[2] });
        BOOST_TEST_REQUIRE(user.updateUser(newSettings));
    }
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());

    for (auto& user : users) {
        BOOST_TEST_REQUIRE(user->pendingUpdate);
        BOOST_TEST_REQUIRE(user->pendingUpdate->blockId == blockchain->getLatestBlockId() + 10);
    }

    // skip 8 blocks, nothing should happen
    BOOST_TEST_REQUIRE(blockchain->addBlock(blockchain->mineBlock(8)));
    for (auto& user : users) {
        BOOST_REQUIRE(user.confirmed()->settings != newSettings);
    }

    // miner one more block, it should update user settings
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    for (auto& user : users) {
        BOOST_REQUIRE(user.confirmed()->settings == newSettings);
        BOOST_TEST_REQUIRE(user->lastSettingsUpdate == blockchain->getLatestBlockId());
    }
}

BOOST_AUTO_TEST_CASE(create_and_update_prefixes)
{
    auto users = user.createUsers(10);
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());

    std::vector<std::string> prefixIds;
    std::string temporaryPrefix = "X1X";
    for (size_t i = kStoragePrefixMinLength; i <= kStoragePrefixMaxLength; ++i) {
        temporaryPrefix += "X";
        prefixIds.push_back(temporaryPrefix);
    }

    for (auto& prefixId : prefixIds) {
        user.createPrefix(prefixId);
        BOOST_TEST_REQUIRE(db->unconfirmed().storage.getPrefix(prefixId));
    }
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());

    // invalid prefixes
    for (auto& prefixId : { ""s, "AAA"s, "@@@@"s, "AAAABBBBCCCCDDDDE"s, "a"s, "XXXXa"s, "XA\0XA"s }) {
        auto result = user.createPrefix(prefixId);
        BOOST_TEST_REQUIRE(!result);
        BOOST_TEST_REQUIRE(!db->unconfirmed().storage.getPrefix(prefixId));
    }

    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());

    for (auto& prefixId : prefixIds) {
        BOOST_TEST_REQUIRE(db->confirmed().storage.getPrefix(prefixId));
    }

    BOOST_TEST_REQUIRE(db->unconfirmed().storage.getPrefixesCount() == prefixIds.size());

    uint16_t allowedUsersSize = 0;
    for (auto& prefixId : prefixIds) {
        std::set<UserId> allowedUsers;
        for (auto& user : users) {
            if (allowedUsers.size() == allowedUsersSize)
                break;
            allowedUsers.insert(user);
        }
        PrefixSettings settings = {
            .allowedUsers = allowedUsers
        };
        user.updatePrefix(prefixId, settings);
        BOOST_TEST_REQUIRE(db->unconfirmed().storage.getPrefix(prefixId)->settings.allowedUsers == allowedUsers);
        allowedUsersSize = std::min<size_t>(allowedUsersSize + 1, kStoragePrefixMaxAllowedUsers);
    }

    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    BOOST_TEST_REQUIRE(db->confirmed().storage.getPrefixesCount() == prefixIds.size());
}

BOOST_AUTO_TEST_CASE(events_basic)
{
    std::promise<Block_cptr> blockPromise;
    std::promise<Transaction_cptr> transactionPromise;
    auto blockFuture = blockPromise.get_future();
    auto transactionFuture = transactionPromise.get_future();

    auto eventListener = blockchain->registerEventsListener(EventsListenerCallbacks{
        .onBlocks = [&](auto blocks, bool didChangeBranch) {
             blockPromise.set_value(blocks[0]);
        },
        .onNewTransactions = [&](auto transactions) {
        transactionPromise.set_value(transactions[0]);
        transactionPromise = std::promise<Transaction_cptr>();
        }
                                                             });

    BOOST_REQUIRE(blockFuture.wait_for(chrono::seconds(0)) == std::future_status::timeout);

    // create user
    auto key = PrivateKey::generate();
    auto transactionStatus = user.createUser(key.publicKey());
    BOOST_TEST_REQUIRE(transactionStatus);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUser(key.publicKey()));

    BOOST_REQUIRE(transactionFuture.wait_for(chrono::seconds(1)) == std::future_status::ready);
    auto transaction = transactionFuture.get();
    BOOST_TEST_REQUIRE(transaction->getId() == transactionStatus.getTransactionId());

    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    BOOST_REQUIRE(blockFuture.wait_for(chrono::seconds(1)) == std::future_status::ready);
    auto block = blockFuture.get();
    BOOST_TEST_REQUIRE(block);
    BOOST_TEST_REQUIRE(block->getId() == 2);

    // unregister event listener
    eventListener = nullptr;

    // create another user and mine next block
    key = PrivateKey::generate();
    transactionStatus = user.createUser(key.publicKey());
    BOOST_TEST_REQUIRE(transactionStatus);
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
}

BOOST_AUTO_TEST_CASE(events_thread_safety)
{
    auto eventListener = blockchain->registerEventsListener(EventsListenerCallbacks{
        .onBlocks = [&](auto blocks, bool didChangeBranch) {
        std::this_thread::sleep_for(chrono::seconds(1));
        },
        .onNewTransactions = [&](auto transactions) {
        std::this_thread::sleep_for(chrono::seconds(1));
        }
                                                            });

    auto keys = PrivateKey::generate(3);
    auto start = chrono::high_resolution_clock::now();
    for (auto& key : keys) {
        user.createUser(key);
    }
    BOOST_REQUIRE((chrono::high_resolution_clock::now() - start) < chrono::seconds(1));
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    BOOST_TEST_REQUIRE(blockchain->mineAndAddBlock());
    BOOST_REQUIRE((chrono::high_resolution_clock::now() - start) < chrono::seconds(3));
}

BOOST_AUTO_TEST_CASE(empty_blocks)
{
    for (uint32_t blockId = 2, depth = 2; blockId < kMinersQueueSize * 2; blockId *= 2, ++depth) {
        auto nextMiners = blockchain->getNextMiners(db->confirmed().blocks.getMinersQueue(), db->confirmed().miners.getTopMiners(),
                                                    blockId - db->confirmed().blocks.getLatestBlockHeader()->getId());
        auto block = Block::create(blockId, depth, nextMiners, {}, db->confirmed().blocks.getLatestBlockHeader()->getHash(),
                                   blockchain->getMinerKey());
        BOOST_TEST_REQUIRE(blockchain->addBlock(block));
    }
}

BOOST_AUTO_TEST_CASE(invalid_blocks)
{
    // missing next miner
    {
        uint32_t blockId = 10;
        auto nextMiners = blockchain->getNextMiners(db->confirmed().blocks.getMinersQueue(), db->confirmed().miners.getTopMiners(),
                                                    blockId - db->confirmed().blocks.getLatestBlockHeader()->getId());
        auto block = Block::create(blockId + 1, db->confirmed().blocks.getLatestBlockHeader()->getDepth() + 1, nextMiners,
                                   {}, db->confirmed().blocks.getLatestBlockHeader()->getHash(), blockchain->getMinerKey());
        BOOST_TEST_REQUIRE(!blockchain->addBlock(block));
    }
    // invalid next miner
    {
        uint32_t blockId = 10;
        auto nextMiners = blockchain->getNextMiners(db->confirmed().blocks.getMinersQueue(), db->confirmed().miners.getTopMiners(),
                                                    blockId - db->confirmed().blocks.getLatestBlockHeader()->getId());
        nextMiners.push_back(MinerId(PrivateKey::generate().publicKey()));
        auto block = Block::create(blockId + 1, db->confirmed().blocks.getLatestBlockHeader()->getDepth() + 1, nextMiners,
                                   {}, db->confirmed().blocks.getLatestBlockHeader()->getHash(), blockchain->getMinerKey());
        BOOST_TEST_REQUIRE(!blockchain->addBlock(block));
    }
    // invalid depth
    {
        uint32_t blockId = 10;
        auto nextMiners = blockchain->getNextMiners(db->confirmed().blocks.getMinersQueue(), db->confirmed().miners.getTopMiners(),
                                                    blockId - db->confirmed().blocks.getLatestBlockHeader()->getId());
        auto block = Block::create(blockId, 1, nextMiners,
                                   {}, db->confirmed().blocks.getLatestBlockHeader()->getHash(), blockchain->getMinerKey());
        BOOST_TEST_REQUIRE(!blockchain->addBlock(block));
    }
    // invalid transaction #1
    {
        uint32_t blockId = 10;
        auto nextMiners = blockchain->getNextMiners(db->confirmed().blocks.getMinersQueue(), db->confirmed().miners.getTopMiners(),
                                                    blockId - db->confirmed().blocks.getLatestBlockHeader()->getId());
        auto createUserTransaction = CreateUserTransaction::create(PrivateKey::generate().publicKey(), 50)->
            setUserId(blockchain->getUserId())->sign({ blockchain->getMinerKey() });
        auto block = Block::create(blockId, db->confirmed().blocks.getLatestBlockHeader()->getDepth() + 1, nextMiners,
                                   { createUserTransaction }, db->confirmed().blocks.getLatestBlockHeader()->getHash(),
                                   blockchain->getMinerKey());
        BOOST_TEST_REQUIRE(!blockchain->addBlock(block));
    }
    // invalid transaction #2
    {
        uint32_t blockId = 10;
        auto nextMiners = blockchain->getNextMiners(db->confirmed().blocks.getMinersQueue(), db->confirmed().miners.getTopMiners(),
                                                    blockId - db->confirmed().blocks.getLatestBlockHeader()->getId());
        auto createUserTransaction = CreateUserTransaction::create(PrivateKey::generate().publicKey(), 50)->
            setUserId(blockchain->getUserId())->sign({ blockchain->getMinerKey() });
        auto block = Block::create(blockId, db->confirmed().blocks.getLatestBlockHeader()->getDepth() + 1, nextMiners,
                                   { createUserTransaction }, db->confirmed().blocks.getLatestBlockHeader()->getHash(),
                                   blockchain->getMinerKey());
        BOOST_TEST_REQUIRE(!blockchain->addBlock(block));
    }
}

BOOST_AUTO_TEST_CASE(change_branch)
{
    BlockchainOptions blockchainOptions = blockchain->getOptions();
    blockchainOptions.firstBlocks.emplace(1, blockchain->getBlock(1));
    BlockchainFixture fixture2(blockchainOptions);
    BOOST_TEST_REQUIRE(fixture2.blockchain->getBlock(1));

    auto block2 = blockchain->mineBlock();
    BOOST_TEST_REQUIRE(blockchain->addBlock(block2));
    BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block2));

    auto block3 = fixture2.blockchain->mineBlock();
    BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block3));

    auto block4 = blockchain->mineBlock(1);
    BOOST_TEST_REQUIRE(blockchain->addBlock(block4));
    auto block5 = blockchain->mineBlock();
    BOOST_TEST_REQUIRE(blockchain->addBlock(block5));

    BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block4, false));
    BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockId() == 3);
    BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getBlockHeader(3));
    BOOST_TEST_REQUIRE(!fixture2.db->confirmed().blocks.getBlockHeader(4));
    BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block5));
    BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockId() == 5);
    BOOST_TEST_REQUIRE(!fixture2.db->confirmed().blocks.getBlockHeader(3));
    BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getBlockHeader(4));
    BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getBlockHeader(5));

    // now both have 5th block, mine 30 new blocks for both instances
    std::vector<Block_cptr> blocks1, blocks2;
    for (size_t i = 0; i < 30; ++i) {
        auto block1 = blockchain->mineBlock(1);
        BOOST_TEST_REQUIRE(blockchain->addBlock(block1));
        auto block2 = fixture2.blockchain->mineBlock();
        BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block2));
        blocks1.push_back(block1);
        blocks2.push_back(block2);
    }

    // reinitialize blockchain for 2st instance, should clear cached blocks
    fixture2.reinitialize();

    // add blocks from 1st instance to 2nd instance
    auto lastBlockHeader2 = fixture2.db->confirmed().blocks.getLatestBlockHeader();
    for (auto& block : blocks1) {
        BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block, false));
        BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockHeader()->getHash() == lastBlockHeader2->getHash());
    }

    // add one more block for 1st instance, and then add it to 2nd instance, should change branch
    {
        auto block = blockchain->mineBlock(1);
        BOOST_TEST_REQUIRE(blockchain->addBlock(block));
        BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block));
        BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockId() == block->getId());
        BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockHeader()->getHash() != lastBlockHeader2->getHash());
        BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockHeader()->getHash() == block->getHeaderHash());
    }

    // add one valid, and one invalid block, and then again one valid block for instance 2
    {
        Block_cptr latestBlock = blocks2.back();
        auto lastValidBlockHeader2 = fixture2.db->confirmed().blocks.getLatestBlockHeader();
        auto userId1 = PrivateKey::generate().publicKey();
        auto userId2 = PrivateKey::generate().publicKey();
        auto createUserTransaction1 = CreateUserTransaction::create(userId1, latestBlock->getId())->
            setUserId(blockchain->getUserId())->sign({ blockchain->getMinerKey() });
        auto createUserTransaction2 = CreateUserTransaction::create(userId2, latestBlock->getId())->
            setUserId(blockchain->getUserId())->sign({ blockchain->getMinerKey() });

        // create valid block, post 2 transactions first to validate rebuild
        BOOST_TEST_REQUIRE(blockchain->postTransaction(createUserTransaction1));
        BOOST_TEST_REQUIRE(blockchain->postTransaction(createUserTransaction2));
        auto block1 = Block::create(latestBlock->getId() + latestBlock->getNextMiners().size(),
                                    latestBlock->getDepth() + latestBlock->getNextMiners().size(), latestBlock->getNextMiners(),
                                    { createUserTransaction1 }, latestBlock->getHeaderHash(), blockchain->getMinerKey());
        latestBlock = block1;

        // create invalid block (duplicated transaction), post transactions first, should fail (duplicated) if rebuild was correct
        auto block2 = Block::create(latestBlock->getId() + latestBlock->getNextMiners().size(),
                                    latestBlock->getDepth() + latestBlock->getNextMiners().size(), latestBlock->getNextMiners(),
                                    { createUserTransaction1, createUserTransaction2 }, latestBlock->getHeaderHash(), blockchain->getMinerKey());

        BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block1, false));
        BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockHeader()->getHash() == lastValidBlockHeader2->getHash());
        BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block2, false));
        BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockHeader()->getHash() == lastValidBlockHeader2->getHash());
        // check if can be added again, it shouldn't be possible, it's banned now
        BOOST_TEST_REQUIRE(!fixture2.blockchain->addBlock(block2, false));

        // create one more valid block, post transactions first, should fail (duplicated) if rebuild was correct
        auto block3 = Block::create(latestBlock->getId() + latestBlock->getNextMiners().size(),
                                    latestBlock->getDepth() + latestBlock->getNextMiners().size(), latestBlock->getNextMiners(),
                                    { createUserTransaction2 }, latestBlock->getHeaderHash(), blockchain->getMinerKey());
        BOOST_TEST_REQUIRE(fixture2.blockchain->addBlock(block3, true));
        BOOST_TEST_REQUIRE(fixture2.db->confirmed().blocks.getLatestBlockHeader()->getHash() == block3->getHeaderHash());
    }
}

BOOST_AUTO_TEST_SUITE_END();

