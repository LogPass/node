#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/block_tree.h>
#include <blockchain/blockchain.h>
#include <blockchain/pending_transactions.h>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/storage/create_prefix.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(block_tree);

BOOST_AUTO_TEST_CASE(general)
{
    auto keys = PrivateKey::generate(5);
    BlockTree tree(std::make_shared<Bans>(), std::make_shared<PendingTransactions>());

    TopMinersSet topMiners;
    auto miner = Miner::create(MinerId(keys[0].publicKey()), UserId(), 1);
    miner->stake = 10;
    miner->settings.api = "0.com"; // for debugging
    topMiners.insert(miner);

    // create first block
    auto queue = Blockchain::getNextMiners({}, topMiners, 240);
    auto block = Block::create(1, 1, queue, {}, Hash(), keys[0]);
    tree.loadBlocks({ block }, queue);

    // test blockIdsAndHashes
    BOOST_TEST_REQUIRE(tree.getBlockIdsAndHashes().size() == 1);
    BOOST_TEST_REQUIRE(tree.getBlockIdsAndHashes()[0].second == block->getHeaderHash());

    // create miners
    for (size_t i = 1; i < keys.size(); ++i) {
        auto miner = Miner::create(MinerId(keys[i].publicKey()), UserId(), 1);
        miner->stake = 10;
        miner->settings.api = std::to_string(i); // for debugging
        topMiners.insert(miner);
    }

    // create another 33 blocks
    for (size_t i = 2; i < 35; ++i) {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        for (auto& nextMiner : nextMiners) {
            queue.pop_front();
            queue.push_back(nextMiner);
        }
        block = Block::create(i, i, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
        auto mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.size() == i);
        BOOST_TEST_REQUIRE(mainBranch.back().block->getHeaderHash() == block->getHeaderHash());
        tree.updateActiveBranch(mainBranch);
    }

    // test getBlockIdsAndHashes
    auto blockIdsAndHashes = tree.getBlockIdsAndHashes();
    auto levels = tree.getLevels();
    BOOST_TEST_REQUIRE(blockIdsAndHashes.size() == 33);
    for (size_t i = 0; i < 33; ++i) {
        auto& [blockId, hash] = blockIdsAndHashes[i];
        BOOST_TEST_REQUIRE(hash == levels[32 - i].begin()->second.getHeaderHash());
        BOOST_TEST_REQUIRE(blockId == levels[32 - i].begin()->second.getId());
    }

    // create more blocks
    for (size_t i = 35; i < 99; ++i) {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        for (auto& nextMiner : nextMiners) {
            queue.pop_front();
            queue.push_back(nextMiner);
        }
        block = Block::create(i, i, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
        auto mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.size() == kDatabaseRolbackableBlocks + 2);
        BOOST_TEST_REQUIRE(mainBranch.back().block->getHeaderHash() == block->getHeaderHash());
        tree.updateActiveBranch(mainBranch);
    }

    // test invalid block, wrong nextMiners #1
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        auto invalidBlock = Block::create(100, 99, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(!tree.addBlock(invalidBlock, MinerId()));
    }

    // test invalid block, wrong nextMiners #2
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 3);
        auto invalidBlock = Block::create(100, 99, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(!tree.addBlock(invalidBlock, MinerId()));
    }

    // test invalid block, wrong nextMiners #3
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 240);
        auto invalidBlock = Block::create(100, 99, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(!tree.addBlock(invalidBlock, MinerId()));
    }

    // test invalid block, wrong kev
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 2);
        auto invalidBlock = Block::create(100, 99, nextMiners, {}, block->getHeaderHash(), keys[1]);
        BOOST_TEST_REQUIRE(!tree.addBlock(invalidBlock, MinerId()));
    }

    // test invalid block, invalid depth #1
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 2);
        auto invalidBlock = Block::create(100, 98, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(!tree.addBlock(invalidBlock, MinerId()));
    }

    // test invalid block, invalid depth #2
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 2);
        auto invalidBlock = Block::create(100, 100, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(!tree.addBlock(invalidBlock, MinerId()));
    }

    // add valid block
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 2);
        for (auto& nextMiner : nextMiners) {
            queue.pop_front();
            queue.push_back(nextMiner);
        }
        block = Block::create(100, 99, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
        auto mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.size() == kDatabaseRolbackableBlocks + 2);
        tree.updateActiveBranch(mainBranch);
    }

    // skip 141 blocks and add two new blocks from other miner
    {
        MinerId minerId = queue[141];
        BOOST_TEST_REQUIRE(minerId != keys[0].publicKey(), keys[0].publicKey());
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 142);
        MinersQueue queue2 = queue;
        for (auto& nextMiner : nextMiners) {
            queue2.pop_front();
            queue2.push_back(nextMiner);
        }
        Block_cptr newBlock;
        for (auto& key : keys) {
            if (key.publicKey() == minerId) {
                newBlock = Block::create(242, 100, nextMiners, {}, block->getHeaderHash(), key);
                BOOST_TEST_REQUIRE(tree.addBlock(newBlock, minerId));
                auto mainBranch = tree.getLongestBranch();
                BOOST_TEST_REQUIRE(mainBranch.back().block->getHeaderHash() == newBlock->getHeaderHash());
                tree.updateActiveBranch(mainBranch);
            }
        }
        minerId = queue2[7];
        nextMiners = Blockchain::getNextMiners(queue2, topMiners, 8);
        for (auto& key : keys) {
            if (key.publicKey() == minerId) {
                newBlock = Block::create(250, 101, nextMiners, {}, newBlock->getHeaderHash(), key);
                BOOST_TEST_REQUIRE(tree.addBlock(newBlock, minerId));
                auto mainBranch = tree.getLongestBranch();
                BOOST_TEST_REQUIRE(mainBranch.back().block->getHeaderHash() == newBlock->getHeaderHash());
                tree.updateActiveBranch(mainBranch);
            }
        }
    }

    // add block on different branch, it shouldn't change longest branch
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        for (auto& nextMiner : nextMiners) {
            queue.pop_front();
            queue.push_back(nextMiner);
        }
        block = Block::create(101, 100, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
        auto mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.back().block->getHeaderHash() != block->getHeaderHash());
        tree.updateActiveBranch(mainBranch);
    }

    // skip 3 blocks, add another block on different branch, it shouldn't change longest branch
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 4);
        for (auto& nextMiner : nextMiners) {
            queue.pop_front();
            queue.push_back(nextMiner);
        }
        block = Block::create(105, 101, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
        auto mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.back().block->getHeaderHash() != block->getHeaderHash());
        tree.updateActiveBranch(mainBranch);
    }

    // skip 9 blocks, add another block on different branch, it should change longest branch and update active branch
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 10);
        for (auto& nextMiner : nextMiners) {
            queue.pop_front();
            queue.push_back(nextMiner);
        }
        block = Block::create(115, 102, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree.addBlockHeader(block->getBlockHeader(), MinerId()).first);
        auto currentBranch = tree.getActiveBranch();
        BOOST_TEST_REQUIRE(currentBranch.back().block->getId() == 250);
        auto mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.back().block->getId() == 250);
        tree.updateActiveBranch(mainBranch);
        currentBranch = tree.getActiveBranch();
        BOOST_TEST_REQUIRE(currentBranch.back().block->getId() == 250);
        BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
        mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.back().block->getId() == 115);
        tree.updateActiveBranch(mainBranch);
        currentBranch = tree.getActiveBranch();
        BOOST_TEST_REQUIRE(currentBranch.back().block->getId() == 115);
        BOOST_TEST_REQUIRE(currentBranch.back().executed == true);
    }

    // skip to new miners
    for (size_t i = 200, depth = 103; i < 242; ++i, ++depth) {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, i - block->getId());
        for (auto& nextMiner : nextMiners) {
            queue.pop_front();
            queue.push_back(nextMiner);
        }
        block = Block::create(i, depth, nextMiners, {}, block->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
        auto mainBranch = tree.getLongestBranch();
        BOOST_TEST_REQUIRE(mainBranch.size() == kDatabaseRolbackableBlocks + 2);
        BOOST_TEST_REQUIRE(mainBranch.back().block->getHeaderHash() == block->getHeaderHash());
        tree.updateActiveBranch(mainBranch);
    }
    BOOST_TEST_REQUIRE(queue[0] != keys[0].publicKey(), keys[0].publicKey());

    // create 5 branches, every with depth 31, one with depth of 33
    uint32_t blockIds[5] = { 241, 241, 241, 241, 241 };
    Hash lastHashes[5] = { block->getHeaderHash(), block->getHeaderHash(),
        block->getHeaderHash(), block->getHeaderHash(), block->getHeaderHash() };
    MinersQueue queues[5] = { queue, queue, queue, queue, queue };
    for (size_t i = 0, depth = block->getDepth() + 1; i < 33; ++i, ++depth) {
        for (size_t minerIndex = 0; minerIndex < keys.size(); ++minerIndex) {
            size_t nextTurn = 0;
            for (; nextTurn < 10; ++nextTurn) {
                if (queues[minerIndex][nextTurn] == keys[minerIndex].publicKey())
                    break;
            }
            BOOST_TEST_REQUIRE(nextTurn < 10);

            if (i == 31) {
                auto currentBranch = tree.getActiveBranch();
                if (currentBranch.back().block->getId() == blockIds[minerIndex])
                    continue;
            } else if (i == 32) {
                auto currentBranch = tree.getActiveBranch();
                if (currentBranch.back().block->getId() != blockIds[minerIndex])
                    continue;
            }

            auto nextMiners = Blockchain::getNextMiners(queues[minerIndex], topMiners, nextTurn + 1);
            for (auto& nextMiner : nextMiners) {
                queues[minerIndex].pop_front();
                queues[minerIndex].push_back(nextMiner);
            }

            auto block = Block::create(blockIds[minerIndex] + nextMiners.size(), depth, nextMiners, {},
                                       lastHashes[minerIndex], keys[minerIndex]);
            blockIds[minerIndex] = block->getId();
            lastHashes[minerIndex] = block->getHeaderHash();
            BOOST_TEST_REQUIRE(tree.addBlockHeader(block->getBlockHeader(), keys[minerIndex].publicKey()).first);
            BOOST_TEST_REQUIRE(tree.addBlock(block, keys[minerIndex].publicKey()));
            auto mainBranch = tree.getLongestBranch();
            if (mainBranch.size() == kDatabaseRolbackableBlocks + 2) {
                tree.updateActiveBranch(mainBranch);
            }

            if (i == 31) { // should change branch
                BOOST_TEST_REQUIRE(mainBranch.size() == kDatabaseRolbackableBlocks + 2);
                auto levels = tree.getLevels();
                BOOST_TEST_REQUIRE(levels[0].size() == 1);
                BOOST_TEST_REQUIRE(levels[mainBranch.size() - 2].size() == 1);
                BOOST_TEST_REQUIRE(levels[mainBranch.size() - 1].size() == 0);
                for (size_t level = 1; level < mainBranch.size() - 2; ++level) {
                    BOOST_TEST_REQUIRE(levels[level].size() == 5);
                }

                // test getBlockIdsAndHashes
                BOOST_TEST_REQUIRE(tree.getBlockIdsAndHashes(100).size() == 100);
                BOOST_TEST_REQUIRE(tree.getBlockIdsAndHashes(200).size() == 31 * 5 + 2);
                break;
            }

            if (i == 32) { // should leave only 1 branch
                BOOST_TEST_REQUIRE(mainBranch.size() == kDatabaseRolbackableBlocks + 2);
                auto levels = tree.getLevels();
                BOOST_TEST_REQUIRE(levels[levels.size() - 1].size() == 0);
                for (size_t level = 0; level < mainBranch.size() - 1; ++level) {
                    BOOST_TEST_REQUIRE(levels[level].size() == 1);
                }

                // test getBlockIdsAndHashes
                BOOST_TEST_REQUIRE(tree.getBlockIdsAndHashes(100).size() == 33);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(ban)
{
    auto keys = PrivateKey::generate(5);

    TopMinersSet topMiners;
    auto miner = Miner::create(MinerId(keys[0].publicKey()), UserId(), 1);
    miner->stake = 10;
    topMiners.insert(miner);

    auto queue = Blockchain::getNextMiners({}, topMiners, 240);
    auto block = Block::create(1, 1, queue, {}, Hash(), keys[0]);

    BlockTree tree(std::make_shared<Bans>(), std::make_shared<PendingTransactions>());
    tree.loadBlocks({ block }, queue);

    // create 5 branches, 30 blocks each
    uint32_t blockIds[5] = { 1, 1, 1, 1, 1 };
    Hash lastHashes[5] = { block->getHeaderHash(), block->getHeaderHash(),
        block->getHeaderHash(), block->getHeaderHash(), block->getHeaderHash() };
    MinersQueue queues[5] = { queue, queue, queue, queue, queue };
    for (size_t i = 0, depth = 2; i < 30; ++i, ++depth) {
        for (size_t branch = 0; branch < 5; ++branch) {
            auto nextMiners = Blockchain::getNextMiners(queues[branch], topMiners, branch + 1);
            for (auto& nextMiner : nextMiners) {
                queues[branch].pop_front();
                queues[branch].push_back(nextMiner);
            }
            blockIds[branch] += nextMiners.size();
            block = Block::create(blockIds[branch], depth, nextMiners, {}, lastHashes[branch], keys[0]);
            lastHashes[branch] = block->getHeaderHash();
            BOOST_TEST_REQUIRE(tree.addBlock(block, MinerId()));
            auto mainBranch = tree.getLongestBranch();
            auto currentBranch = tree.getActiveBranch();
            if (mainBranch.size() > currentBranch.size()) {
                tree.updateActiveBranch(mainBranch);
            }
        }
    }

    for (size_t i = 1; i < 31; ++i) {
        BOOST_TEST_REQUIRE(tree.getLevels()[i].size() == 5);
    }

    // ban latest block
    BOOST_TEST_REQUIRE(tree.getLevels()[30].size() == 5);
    tree.banBlock(block->getHeaderHash(), "test");
    BOOST_TEST_REQUIRE(tree.getLevels()[30].size() == 4);
    BOOST_TEST_REQUIRE(!tree.addBlock(block, MinerId()));

    auto levels = tree.getLevels();
    Hash toBan = levels[15].begin()->second.executed ? levels[15].rbegin()->first : levels[15].begin()->first;
    tree.banBlock(toBan, "test");
    for (size_t i = 1; i < 15; ++i) {
        BOOST_TEST_REQUIRE(tree.getLevels()[i].size() == 5);
    }
    for (size_t i = 15; i < 30; ++i) {
        BOOST_TEST_REQUIRE(tree.getLevels()[i].size() == 4);
    }

    for (auto& node : levels[1]) {
        if (node.second.executed)
            continue;
        tree.banBlock(node.first, "test");
    }

    levels = tree.getLevels();
    for (size_t i = 0; i < 31; ++i) {
        BOOST_TEST_REQUIRE(levels[i].size() == 1);
    }
}

BOOST_AUTO_TEST_CASE(pending_block)
{
    auto keys = PrivateKey::generate(5);

    TopMinersSet topMiners;
    auto miner = Miner::create(MinerId(keys[0].publicKey()), UserId(), 1);
    miner->stake = 10;
    topMiners.insert(miner);

    auto queue = Blockchain::getNextMiners({}, topMiners, 240);
    auto block = Block::create(1, 1, queue, {}, Hash(), keys[0]);

    auto bans = std::make_shared<Bans>();
    auto pendingTransactions = std::make_shared<PendingTransactions>();
    auto tree = std::make_shared<BlockTree>(bans, pendingTransactions);
    tree->loadBlocks({ block }, queue);

    // block without transactions
    {
        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        block = Block::create(2, 2, nextMiners, {}, block->getHeaderHash(), keys[0]);

        BOOST_TEST_REQUIRE(tree->addBlockHeader(block->getBlockHeader(), MinerId()).first);
        PendingBlock_ptr pendingBlock = tree->getPendingBlock(block->getBlockHeader()->getHash());
        BOOST_TEST_REQUIRE(pendingBlock);
        BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 2);
    }

    // block with transactions
    {
        std::vector<Transaction_cptr> transactions = {
            CreateUserTransaction::create(keys[0].publicKey(), 1)->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            StorageCreatePrefixTransaction::create(1, -1, "XXXXXXXX")->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
        };

        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        block = Block::create(3, 3, nextMiners, transactions, block->getHeaderHash(), keys[0]);

        BOOST_TEST_REQUIRE(tree->addBlockHeader(block->getBlockHeader(), MinerId()).first);
        PendingBlock_ptr pendingBlock = tree->getPendingBlock(block->getBlockHeader()->getHash());
        BOOST_TEST_REQUIRE(pendingBlock);
        BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 2);
        for (auto& blockTransactionIds : block->getBlockTransactionIds()) {
            BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds) ==
                               PendingBlock::AddResult::CORRECT);
        }
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 2);
        for (auto& transaction : transactions) {
            pendingTransactions->addTransactionIfRequested(transaction);
        }
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 2);
        tree->updateActiveBranch(tree->getLongestBranch());
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 3);
    }

    // block with transactions existing in pending transactions
    {
        std::vector<Transaction_cptr> transactions = {
            CreateUserTransaction::create(keys[0].publicKey(), 2)->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            StorageCreatePrefixTransaction::create(2, -1, "XXXXXXXX")->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
        };

        for (auto& transaction : transactions) {
            pendingTransactions->addTransaction(transaction, MinerId());
        }

        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        block = Block::create(4, 4, nextMiners, transactions, block->getHeaderHash(), keys[0]);

        BOOST_TEST_REQUIRE(tree->addBlockHeader(block->getBlockHeader(), MinerId()).first);
        PendingBlock_ptr pendingBlock = tree->getPendingBlock(block->getBlockHeader()->getHash());
        BOOST_TEST_REQUIRE(pendingBlock);
        BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 3);
        for (auto& blockTransactionIds : block->getBlockTransactionIds()) {
            BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds) ==
                               PendingBlock::AddResult::CORRECT);
        }
        tree->updateActiveBranch(tree->getLongestBranch());
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 4);
    }

    // block with new transaction and transaction existing in pending transactions
    {
        std::vector<Transaction_cptr> transactions = {
            CreateUserTransaction::create(keys[0].publicKey(), 3)->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            StorageCreatePrefixTransaction::create(3, -1, "XXXXXXXX")->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
        };

        pendingTransactions->addTransaction(transactions[0], MinerId());

        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        block = Block::create(5, 5, nextMiners, transactions, block->getHeaderHash(), keys[0]);

        BOOST_TEST_REQUIRE(tree->addBlockHeader(block->getBlockHeader(), MinerId()).first);
        PendingBlock_ptr pendingBlock = tree->getPendingBlock(block->getBlockHeader()->getHash());
        BOOST_TEST_REQUIRE(pendingBlock);
        BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 4);
        for (auto& blockTransactionIds : block->getBlockTransactionIds()) {
            BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds) ==
                               PendingBlock::AddResult::CORRECT);
        }
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 4);
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 1);
        pendingTransactions->addTransaction(transactions[1], MinerId());
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 0);
        tree->updateActiveBranch(tree->getLongestBranch());
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 5);
    }

    // multpile blocks with same transactions (same branch)
    {
        std::vector<Transaction_cptr> transactions = {
            CreateUserTransaction::create(keys[0].publicKey(), 4)->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            CreateUserTransaction::create(keys[1].publicKey(), 4)->setUserId(keys[1].publicKey())->setPublicKey(keys[1].publicKey())->sign({ keys[1] }),
            StorageCreatePrefixTransaction::create(4, -1, "XXXXXXXX")->setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            StorageCreatePrefixTransaction::create(4, -1, "YYYYYYYY")->setUserId(keys[1].publicKey())->setPublicKey(keys[1].publicKey())->sign({ keys[1] }),
        };

        pendingTransactions->addTransaction(transactions[0], MinerId());

        std::vector<Block_cptr> blocks;
        for (size_t i = 0; i < 10; ++i) {
            auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
            block = Block::create(6 + i, 6 + i, nextMiners, transactions, block->getHeaderHash(), keys[0]);
            BOOST_TEST_REQUIRE(tree->addBlockHeader(block->getBlockHeader(), MinerId()).first);
            blocks.push_back(block);
        }

        for (auto& block : blocks) {
            PendingBlock_ptr pendingBlock = tree->getPendingBlock(block->getBlockHeader()->getHash());
            BOOST_TEST_REQUIRE(pendingBlock);
            BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
            for (auto& blockTransactionIds : block->getBlockTransactionIds()) {
                BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds) ==
                                   PendingBlock::AddResult::CORRECT);
            }
        }

        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 3);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 5);
        pendingTransactions->addTransaction(transactions[1], MinerId());
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 2);
        pendingTransactions->addTransactionIfRequested(transactions[2]);
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 1);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 5);
        pendingTransactions->addTransaction(transactions[3], MinerId());
        for (int i = 6; i <= 15; ++i) {
            tree->updateActiveBranch(tree->getLongestBranch());
            BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == i);
        }
        tree->updateActiveBranch(tree->getLongestBranch());
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 15);
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 0);
    }

    // multpile blocks with same transactions (different branches)
    {
        std::vector<Transaction_cptr> transactions = {
            CreateUserTransaction::create(keys[0].publicKey(), 5)->
            setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            CreateUserTransaction::create(keys[1].publicKey(), 5)->
            setUserId(keys[1].publicKey())->setPublicKey(keys[1].publicKey())->sign({ keys[1] }),
            StorageCreatePrefixTransaction::create(5, -1, "XXXXXXXX")->
            setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            StorageCreatePrefixTransaction::create(5, -1, "YYYYYYYY")->
            setUserId(keys[1].publicKey())->setPublicKey(keys[1].publicKey())->sign({ keys[1] }),
        };

        pendingTransactions->addTransaction(transactions[0], MinerId());

        std::vector<Block_cptr> blocks;
        for (size_t i = 0; i < 10; ++i) {
            auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1 + i);
            auto newBlock = Block::create(16 + i, 16, nextMiners, transactions, block->getHeaderHash(), keys[0]);
            BOOST_TEST_REQUIRE(tree->addBlockHeader(newBlock->getBlockHeader(), MinerId()).first);
            blocks.push_back(newBlock);
        }

        for (auto& block : blocks) {
            PendingBlock_ptr pendingBlock = tree->getPendingBlock(block->getBlockHeader()->getHash());
            BOOST_TEST_REQUIRE(pendingBlock);
            BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
            for (auto& blockTransactionIds : block->getBlockTransactionIds()) {
                BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds) ==
                                   PendingBlock::AddResult::CORRECT);
            }
        }

        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 3);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 15);
        pendingTransactions->addTransaction(transactions[1], MinerId());
        pendingTransactions->addTransactionIfRequested(transactions[2]);
        BOOST_TEST_REQUIRE(tree->getActiveBranch().size() == 15);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 15);
        pendingTransactions->addTransaction(transactions[3], MinerId());
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 16);
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 0);
    }

    // create 2 pending blocks, ban first, second should be removed by cleanup
    {
        std::vector<Transaction_cptr> transactions = {
            CreateUserTransaction::create(keys[0].publicKey(), 6)->
            setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] }),
            StorageCreatePrefixTransaction::create(6, -1, "XXXXXXXX")->
            setUserId(keys[0].publicKey())->setPublicKey(keys[0].publicKey())->sign({ keys[0] })
        };

        auto nextMiners = Blockchain::getNextMiners(queue, topMiners, 1);
        auto block1 = Block::create(16, 16, nextMiners, transactions, block->getHeaderHash(), keys[0]);
        auto block2 = Block::create(17, 17, nextMiners, transactions, block1->getHeaderHash(), keys[0]);
        BOOST_TEST_REQUIRE(tree->addBlockHeader(block1->getBlockHeader(), MinerId()).first);
        BOOST_TEST_REQUIRE(tree->addBlockHeader(block2->getBlockHeader(), MinerId()).first);

        for (auto& block : { block1, block2 }) {
            PendingBlock_ptr pendingBlock = tree->getPendingBlock(block->getBlockHeader()->getHash());
            BOOST_TEST_REQUIRE(pendingBlock);
            BOOST_TEST_REQUIRE(pendingBlock->addBlockBody(block->getBlockBody()) == PendingBlock::AddResult::CORRECT);
            for (auto& blockTransactionIds : block->getBlockTransactionIds()) {
                BOOST_TEST_REQUIRE(pendingBlock->addBlockTransactionIds(blockTransactionIds) ==
                                   PendingBlock::AddResult::CORRECT);
            }
        }

        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 2);
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 16);
        tree->banBlock(block1->getHeaderHash(), "test");
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 16);
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 0);
        pendingTransactions->addTransaction(transactions[0], MinerId());
        pendingTransactions->addTransaction(transactions[1], MinerId());
        BOOST_TEST_REQUIRE(tree->getLongestBranch().size() == 16);
        BOOST_TEST_REQUIRE(pendingTransactions->getRequestedTransactionsCount() == 0);
    }
}

BOOST_AUTO_TEST_SUITE_END();
