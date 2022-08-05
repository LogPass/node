#include "pch.h"
#include "block_tree.h"

namespace logpass {

BlockTree::BlockTree(const std::shared_ptr<Bans>& bans, const std::shared_ptr<PendingTransactions>& pendingTransactions)
    : m_bans(bans), m_pendingTransactions(pendingTransactions), m_loggerId("")
{
    ASSERT(bans && m_pendingTransactions);
    m_levels.resize(DEPTH);

    m_logger.add_attribute("Class", boost::log::attributes::constant<std::string>("BlockTree"));
    m_logger.add_attribute("ID", m_loggerId);
}

BlockTree::~BlockTree()
{
    // clear all pending blocks
    for (auto& level : m_levels) {
        for (auto it = level.begin(); it != level.end(); ) {
            if (it->second.pendingBlock) {
                clearPendingBlock(it->second);
                it = level.erase(it);
                continue;
            }
            ++it;
        }
    }
}

void BlockTree::loadBlocks(const std::deque<Block_cptr>& blocks, const MinersQueue& miningQueue)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    ASSERT(!blocks.empty());
    ASSERT(m_levels[0].size() == 0 && m_miningQueue.empty());
    ASSERT(miningQueue.size() == kMinersQueueSize);

    m_loggerId.set(blocks.back()->toString());
    LOG_CLASS(debug) << "loadBlocks " << blocks.size() << " (first: " << blocks.front()->toString() << ")";

    size_t firstBlocks = 0;
    if (blocks.size() >= m_levels.size()) {
        firstBlocks = blocks.size() - (m_levels.size() - 1);
    }

    m_miningQueue = miningQueue;
    // adjust mining queue if needed
    for (size_t i = 0; i < firstBlocks; ++i) {
        MinersQueue nextMiners = blocks[i]->getNextMiners();
        m_miningQueue.insert(miningQueue.end(), nextMiners.begin(), nextMiners.end());
    }
    // limit mining queue to kMinersQueueSize entries
    if (m_miningQueue.size() > kMinersQueueSize) {
        m_miningQueue.erase(miningQueue.begin(), std::next(miningQueue.begin(), miningQueue.size() - kMinersQueueSize));
    }

    for (size_t i = firstBlocks, level = 0; i < blocks.size(); ++i, ++level) {
        MinerId miner(blocks[i]->getBlockHeader()->getMiner());
        m_levels[level].emplace(blocks[i]->getHeaderHash(),
                                BlockTreeNode(blocks[i], nullptr, true, MinerId(), miner));
    }

#ifndef NDEBUG
    // make sure block ids and mining queue are valid
    for (size_t index = 1, blockIndex = 0; index < m_levels.size(); ++index) {
        auto& level = m_levels[index];
        if (level.empty()) {
            break;
        }
        ASSERT(m_levels[index - 1].begin()->second.getId() < level.begin()->second.getId());
        auto& block = level.begin()->second.block;
        blockIndex += block->getSkippedBlocks();
        if (blockIndex >= m_miningQueue.size()) {
            break;
        }
        ASSERT(m_miningQueue[blockIndex] == block->getBlockHeader()->getMiner());
        blockIndex += 1;
    }
#endif
}

std::pair<PendingBlock_ptr, bool> BlockTree::addBlockHeader(const BlockHeader_cptr& blockHeader,
                                                            const MinerId& reporter)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    LOG_CLASS(debug) << "addBlockHeader " << blockHeader->toString() << ", reporter: " << reporter;

    if (isBanned(blockHeader->getHash(), reporter)) {
        LOG_CLASS(debug) << "block header is banned";
        return { nullptr, false };
    }

    auto levelIterator = std::find_if(m_levels.begin(), m_levels.end(), [&](auto& level) {
        return level.count(blockHeader->getPrevHeaderHash()) != 0;
    });

    if (levelIterator == m_levels.end()) {
        LOG_CLASS(debug) << "block header parent doesn't exists";
        return { nullptr, false };
    }

    auto nextLevelIterator = std::next(levelIterator);
    if (nextLevelIterator == m_levels.end()) {
        LOG_CLASS(debug) << "block header parent is in last level";
        return { nullptr, false };
    }

    auto blockIterator = nextLevelIterator->find(blockHeader->getHash());
    if (blockIterator != nextLevelIterator->end()) {
        if (blockIterator->second.pendingBlock) {
            LOG_CLASS(debug) << "pending block already exists";
            return { blockIterator->second.pendingBlock , true };
        } else {
            LOG_CLASS(debug) << "block already exists";
            return { nullptr , true };
        }
    }

    // currently allow only 1 unexecuted reported block per repoter for given level
    for (auto& node : *nextLevelIterator) {
        if (node.second.reporter != reporter || !reporter.isValid())
            continue;
        if (!node.second.executed) {
            LOG_CLASS(debug) << "there's other reported block with the same depth by that miner";
            return { nullptr, false };
        }
    }

    auto parentIterator = levelIterator->find(blockHeader->getPrevHeaderHash());
    if (parentIterator->second.getId() + blockHeader->getSkippedBlocks() + 1 != blockHeader->getId()) {
        LOG_CLASS(debug) << "block header id is invalid";
        return { nullptr, false };
    }

    if (parentIterator->second.getDepth() + 1 != blockHeader->getDepth()) {
        LOG_CLASS(debug) << "block header depth is invalid";
        return { nullptr, false };
    }

    MinerId expectedMinerId = getExpectedMinerId(blockHeader->getPrevHeaderHash(), blockHeader->getSkippedBlocks());
    if (!blockHeader->validate(expectedMinerId)) {
        LOG_CLASS(debug) << "block header validation failed";
        return { nullptr, false };
    }

    std::shared_ptr<PendingBlock> pendingBlock = std::make_shared<PendingBlock>(
        blockHeader, expectedMinerId, std::bind(&BlockTree::onPendingBlockUpdated, this, std::placeholders::_1));
    LOG_CLASS(debug) << "Created new pending block " << pendingBlock->toString();

    nextLevelIterator->emplace(blockHeader->getHash(),
                               BlockTreeNode(nullptr, pendingBlock, false, reporter, expectedMinerId));
    return { pendingBlock, false };
}

PendingBlock_ptr BlockTree::getPendingBlock(const Hash& hash) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto levelIterator = std::find_if(m_levels.begin(), m_levels.end(), [&](auto& level) {
        return level.count(hash) != 0;
    });

    if (levelIterator == m_levels.end()) {
        return nullptr;
    }

    auto blockIterator = levelIterator->find(hash);
    ASSERT(blockIterator != levelIterator->end());

    return blockIterator->second.pendingBlock;
}

bool BlockTree::addBlock(const Block_cptr& block, const MinerId& reporter)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    LOG_CLASS(debug) << "addBlock " << block->toString() << ", reporter: " << reporter;

    ASSERT(!m_levels[0].empty());

    if (isBanned(block->getHeaderHash(), reporter)) {
        LOG_CLASS(debug) << "block is banned";
        return false;
    }

    auto levelIterator = std::find_if(m_levels.begin(), m_levels.end(), [&](auto& level) {
        return level.count(block->getPrevHeaderHash()) != 0;
    });

    if (levelIterator == m_levels.end()) {
        LOG_CLASS(debug) << "block parent is missing";
        return false;
    }

    auto nextLevelIterator = std::next(levelIterator);
    if (nextLevelIterator == m_levels.end()) {
        LOG_CLASS(debug) << "block parent is in last level";
        return false;
    }

    auto blockIterator = nextLevelIterator->find(block->getHeaderHash());
    if (blockIterator != nextLevelIterator->end()) {
        // block or pending block already exists
        if (blockIterator->second.block == nullptr) {
            LOG_CLASS(debug) << "pending block already exists, replacing with ready block";
            clearPendingBlock(blockIterator->second);
            ASSERT(blockIterator->second.pendingBlock == nullptr);
            blockIterator->second.block = block;
        } else {
            LOG_CLASS(debug) << "block already exists, ignoring";
        }

        return true;
    }

    for (auto& node : *nextLevelIterator) {
        if (node.second.reporter != reporter || !reporter.isValid()) {
            continue;
        }
        if (!node.second.executed) {
            LOG_CLASS(debug) << "there's other reported block with the same depth by that miner";
            return false;
        }
    }

    auto parentIterator = levelIterator->find(block->getPrevHeaderHash());
    if (parentIterator->second.getId() + block->getSkippedBlocks() + 1 != block->getId()) {
        LOG_CLASS(debug) << "block id is invalid";
        return false;
    }

    if (parentIterator->second.getDepth() + 1 != block->getDepth()) {
        LOG_CLASS(debug) << "block depth is invalid";
        return false;
    }

    MinerId expectedMinerId = getExpectedMinerId(block->getPrevHeaderHash(), block->getSkippedBlocks());
    if (block->getMinerId() != expectedMinerId) {
        LOG_CLASS(debug) << "block has invalid miner id";
        return false;
    }

    if (!block->validate(expectedMinerId, block->getPrevHeaderHash())) {
        LOG_CLASS(debug) << "validation error";
        return false;
    }

    nextLevelIterator->emplace(block->getHeaderHash(),
                               BlockTreeNode(block, nullptr, false, reporter, expectedMinerId));
    return true;
}

std::deque<BlockTreeNode> BlockTree::getActiveBranch() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    std::deque<BlockTreeNode> ret;
    for (auto& level : m_levels) {
        auto it = std::find_if(level.begin(), level.end(), [&](auto& entry) {
            return entry.second.executed;
        });
        if (it == level.end()) {
            break;
        }
        ret.push_back(it->second);
    }

    return ret;
}

std::deque<BlockTreeNode> BlockTree::getLongestBranch() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto it = m_levels.rbegin(); it != m_levels.rend(); ++it) {
        auto executedBlockIterator = std::find_if(it->begin(), it->end(), [](auto& entry) {
            return entry.second.executed;
        });

        if (executedBlockIterator == it->end()) {
            continue;
        }

        if (it != m_levels.rbegin()) {
            --it;
            for (auto& entry : *it) {
                if (entry.second.block) {
                    auto parents = getParents(entry.second.getHeaderHash(), true, true, true);
                    if (!parents.empty()) {
                        return parents;
                    }
                }
            }
            ++it;
        }

        return getParents(executedBlockIterator->second.getHeaderHash(), true, true, true);
    }

    return {};
}

void BlockTree::updateActiveBranch(const std::deque<BlockTreeNode>& newBranch)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    LOG_CLASS(debug) << "updateActiveBranch to " << newBranch.back().block->toString();

    ASSERT(m_levels[0].size() == 1);
    ASSERT(m_levels[0].begin()->second.block->getHeaderHash() == newBranch.begin()->block->getHeaderHash());
    ASSERT(newBranch.size() <= m_levels.size());

    for (auto& level : m_levels) {
        for (auto& node : level) {
            node.second.executed = false;
        }
    }

    // set executed nodes
    m_levels[0].begin()->second.executed = true;
    for (size_t i = 1; i < newBranch.size(); ++i) {
        ASSERT(newBranch[i - 1].getId() < newBranch[i].getId());
        auto it = m_levels[i].emplace(newBranch[i].getHeaderHash(), newBranch[i]).first;
        if (it->second.pendingBlock) {
            clearPendingBlock(it->second);
            ASSERT(it->second.pendingBlock == nullptr);
        }
        it->second.block = newBranch[i].block;
        it->second.executed = true;
    }

    m_loggerId.set(newBranch.back().block->toString());

    if (newBranch.size() > kDatabaseRolbackableBlocks + 1) {
        size_t levelsToRemove = newBranch.size() - (kDatabaseRolbackableBlocks + 1);
        for (size_t i = 0; i < levelsToRemove; ++i) {
            // update first level and mining queue
            ASSERT(m_levels.front().size() == 1);
            m_levels.pop_front();

            // remove other level 0 nodes
            auto& frontLevel = m_levels.front();
            for (auto it = frontLevel.begin(); it != frontLevel.end(); ) {
                if (it->second.executed) {
                    ++it;
                } else {
                    clearPendingBlock(it->second);
                    it = frontLevel.erase(it);
                }
            }

            MinersQueue nextMiners = frontLevel.begin()->second.getNextMiners();
            m_miningQueue.insert(m_miningQueue.end(), nextMiners.begin(), nextMiners.end());
            if (m_miningQueue.size() > kMinersQueueSize) {
                m_miningQueue.erase(m_miningQueue.begin(),
                                    std::next(m_miningQueue.begin(), m_miningQueue.size() - kMinersQueueSize));
            }
            ASSERT(m_miningQueue.size() == kMinersQueueSize);
            m_levels.emplace_back();
        }
        cleanup();
    }
}

std::vector<std::pair<uint32_t, Hash>> BlockTree::getBlockIdsAndHashes(uint32_t limit,
                                                                       uint32_t maxBlockDepth) const
{
    ASSERT(limit > 0 && limit < 10000);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<std::pair<uint32_t, Hash>> ret;

    if (m_levels.empty()) {
        return ret;
    }

    auto level = m_levels.rbegin();
    ++level; // skip last level
    for (; level != m_levels.rend(); ++level) {
        // find executed block
        auto it = std::find_if(level->begin(), level->end(), [&](auto& entry) {
            return entry.second.executed;
        });

        if (it != level->end()) {
            if (maxBlockDepth != 0 && it->second.getDepth() > maxBlockDepth) {
                continue;
            }

            ret.push_back({ it->second.getId(), it->second.getHeaderHash() });
            if (ret.size() >= limit) {
                return ret;
            }
        }

        for (auto& node : *level) {
            if (!node.second.block || node.second.executed) {
                continue;
            }
            if (maxBlockDepth != 0 && node.second.getDepth() > maxBlockDepth) {
                continue;
            }
            ret.push_back({ node.second.getId(), node.second.getHeaderHash() });
            if (ret.size() >= limit) {
                return ret;
            }
        }
    }
    return ret;
}

bool BlockTree::hasBlock(const Hash& hash) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto levelIterator = std::find_if(m_levels.begin(), m_levels.end(), [&](auto& level) {
        return level.contains(hash) && level.find(hash)->second.block;
    });
    return levelIterator != m_levels.end();
}

bool BlockTree::isInLastLevel(const Hash& blockHash) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_levels.rbegin()->contains(blockHash) && m_levels.rbegin()->find(blockHash)->second.block;
}

void BlockTree::banBlock(const Hash& hash, const std::string& reason)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    LOG_CLASS(info) << "banBlock " << hash << ", reason: " << reason;

    m_bannedBlocks.insert(hash);

    auto levelIterator = std::find_if(m_levels.begin(), m_levels.end(), [&](auto& level) {
        return level.contains(hash);
    });

    if (levelIterator == m_levels.end()) {
        return;
    }

    auto nodeIterator = levelIterator->find(hash);
    ASSERT(nodeIterator != levelIterator->end());
    ASSERT(nodeIterator->second.executed == false);

    if (nodeIterator->second.reporter.isValid()) {
        LOG_CLASS(info) << "banning reporter " << nodeIterator->second.reporter;
        m_bannedReporters.insert(nodeIterator->second.reporter);
    }

    clearPendingBlock(nodeIterator->second);
    levelIterator->erase(nodeIterator);

    cleanup();
}

bool BlockTree::isBanned(const Hash& blockHash, const MinerId& minerId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (minerId.isValid() && m_bannedReporters.count(minerId)) {
        return true; // block is banned
    }

    if (blockHash.isValid() && m_bannedBlocks.count(blockHash)) {
        return true; // block is banned
    }

    return false;
}

std::deque<std::map<Hash, BlockTreeNode>> BlockTree::getLevels()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_levels;
}

json BlockTree::getDebugInfo() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<json> levels;
    for (auto& level : m_levels) {
        std::vector<json> nodes;
        for (auto& node : level) {
            nodes.push_back({
                { "hash", node.first },
                { "is_executed", node.second.executed },
                { "is_pending", !!node.second.pendingBlock },
                { "id", node.second.getId() },
                { "depth", node.second.getDepth() },
                { "prev_hash", node.second.getPrevHeaderHash() },
                { "reporter", node.second.reporter },
                { "miner", node.second.miner }
                            });
        }
        levels.push_back(std::move(nodes));
    }
    return levels;
}

uint32_t BlockTree::getBaseDepth() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_levels[0].begin()->second.getDepth();
}

MinerId BlockTree::getExpectedMinerId(const Hash& parentHash, size_t skippedBlocks) const
{
    auto parents = getParents(parentHash, false, true, false);

    size_t minerIndex = parents.size() + skippedBlocks;
    for (auto& parent : parents) {
        minerIndex += parent.getSkippedBlocks();
    }

    MinerId expectedMinerId;
    if (minerIndex < m_miningQueue.size()) {
        expectedMinerId = m_miningQueue[minerIndex];
    } else {
        minerIndex -= m_miningQueue.size();
        for (auto& parent : parents) {
            auto parentNextMiners = parent.getNextMiners();
            if (minerIndex < parentNextMiners.size()) {
                expectedMinerId = parentNextMiners[minerIndex];
                break;
            } else {
                minerIndex -= parentNextMiners.size();
            }
        }
    }

    return expectedMinerId;
}

std::deque<BlockTreeNode> BlockTree::getParents(const Hash& blockHeaderHash, bool withoutPendingBlocks,
                                                bool includeItself, bool includeFirstLevel) const
{
    auto levelIterator = std::find_if(m_levels.rbegin(), m_levels.rend(), [&](auto& level) {
        return level.count(blockHeaderHash) != 0;
    });

    if (levelIterator == m_levels.rend()) {
        return {};
    }

    std::deque<BlockTreeNode> ret;
    for (Hash nextHash = blockHeaderHash; levelIterator != m_levels.rend(); ++levelIterator) {
        auto it = levelIterator->find(nextHash);
        if (it == levelIterator->end()) {
            return {};
        }

        if (withoutPendingBlocks && it->second.block == nullptr) {
            return {};
        }

        ret.push_front(it->second);
        nextHash = it->second.getPrevHeaderHash();
    }

    if (!includeItself) {
        ret.pop_back();
    }

    if (!includeFirstLevel && !ret.empty()) {
        ret.pop_front();
    }

    return ret;
}

void BlockTree::onPendingBlockUpdated(const PendingBlock_ptr& pendingBlock)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    LOG_CLASS(debug) << "onPendingBlockUpdated " << pendingBlock->toString();

    if (pendingBlock->isExpired()) {
        LOG_CLASS(trace) << "pending block is expired";
        return;
    }

    auto levelIterator = std::find_if(m_levels.begin(), m_levels.end(), [&](auto& level) {
        return level.count(pendingBlock->getHeaderHash()) != 0;
    });

    if (levelIterator == m_levels.end()) {
        LOG_CLASS(warning) << "pending block " << pendingBlock->toString() << " has not been found in BlockTree";
        return;
    }

    auto blockIterator = levelIterator->find(pendingBlock->getHeaderHash());
    ASSERT(blockIterator != levelIterator->end());
    if (blockIterator->second.pendingBlock == nullptr) {
        LOG_CLASS(warning) << "pending block " << pendingBlock->toString() << " was null in BlockTree node";
        return;
    }

    if (blockIterator->second.pendingBlock != pendingBlock) {
        LOG_CLASS(warning) << "pending block " << pendingBlock->toString() << " was duplicated in BlockTree node";
        return;
    }

    if (pendingBlock->isInvalid()) {
        banBlock(pendingBlock->getHeaderHash(), "pending block is invalid");
        return;
    }

    auto& node = blockIterator->second;
    if (pendingBlock->getStatus() == PendingBlock::Status::MISSING_TRANSACTIONS) {
        if (!node.hasLockedTransactions) {
            node.hasLockedTransactions = true;
            // this may change block missing part to none, so missing part should be check again
            m_pendingTransactions->addPendingBlock(blockIterator->second.pendingBlock);
        }
    }

    if (pendingBlock->getStatus() == PendingBlock::Status::COMPLETE) {
        LOG_CLASS(trace) << "pending block is complete";
        if (!finishPendingBlock(blockIterator->second))
            pendingBlock->setInvalid();
        ASSERT(pendingBlock->isExpired());
    }
}

bool BlockTree::finishPendingBlock(BlockTreeNode& node)
{
    LOG_CLASS(debug) << "finishPendingBlock " << node.pendingBlock->toString();

    ASSERT(node.pendingBlock && !node.block);
    ASSERT(node.hasLockedTransactions || node.pendingBlock->getTransactionIds().empty());
    ASSERT(node.pendingBlock->getStatus() == PendingBlock::Status::COMPLETE);

    PendingBlock_ptr pendingBlock = node.pendingBlock;
    node.block = pendingBlock->createBlock();
    ASSERT(node.block);
    clearPendingBlock(node);

    if (!node.block->validate(node.miner, node.block->getPrevHeaderHash())) {
        LOG_CLASS(warning) << "Block " << node.block->toString() << " is invalid";
        pendingBlock->setInvalid();
        banBlock(node.block->getHeaderHash(), "validation of created block failed");
        return false;
    }

    pendingBlock->setFinished();
    return true;
}

void BlockTree::clearPendingBlock(BlockTreeNode& node)
{
    if (!node.pendingBlock) {
        return;
    }

    node.pendingBlock->setExpired();

    if (node.hasLockedTransactions) {
        m_pendingTransactions->removePendingBlock(node.pendingBlock);
        node.hasLockedTransactions = false;
    }

    node.pendingBlock = nullptr;
}

void BlockTree::cleanup()
{
    for (size_t level = 1; level < m_levels.size(); ++level) {
        for (auto it = m_levels[level].begin(); it != m_levels[level].end(); ) {
            if (m_levels[level - 1].count(it->second.getPrevHeaderHash())) {
                ++it;
            } else {
                clearPendingBlock(it->second);
                it = m_levels[level].erase(it);
            }
        }
    }
}

}
