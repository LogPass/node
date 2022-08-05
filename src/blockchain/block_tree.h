#pragma once

#include "bans.h"
#include "block/block.h"
#include "block/pending_block.h"
#include "block_tree_node.h"
#include "pending_transactions.h"

namespace logpass {

// Manages blocks and pending blocks
class BlockTree {
public:
    static constexpr size_t DEPTH = kDatabaseRolbackableBlocks + 2 + 8;

    BlockTree(const std::shared_ptr<Bans>& bans, const std::shared_ptr<PendingTransactions>& pendingTransactions);
    ~BlockTree();
    BlockTree(const BlockTree&) = delete;
    BlockTree& operator=(const BlockTree&) = delete;

    // loads executed blocks (active branch), mining queue should have next miners after first block
    // should be called only once
    void loadBlocks(const std::deque<Block_cptr>& blocks, const MinersQueue& miningQueue);

    // adds block header, return true if its valid, bool in std::pair is true if block or block header already exists
    std::pair<PendingBlock_ptr, bool> addBlockHeader(const BlockHeader_cptr& blockHeader, const MinerId& reporter);

    // returns pending block
    PendingBlock_ptr getPendingBlock(const Hash& hash) const;

    // adds block
    bool addBlock(const Block_cptr& block, const MinerId& reporter);

    // returns active branch
    std::deque<BlockTreeNode> getActiveBranch() const;

    // returns longest branch
    std::deque<BlockTreeNode> getLongestBranch() const;

    // updates active branch
    void updateActiveBranch(const std::deque<BlockTreeNode>& newBranch);

    // returns block ids and hashes, sorted by depth of block in block tree, last level is first, first level is last
    // it's used by GetBlockHeader packet to get next block candidate for block tree
    std::vector<std::pair<uint32_t, Hash>> getBlockIdsAndHashes(uint32_t limit = 100,
                                                                       uint32_t maxBlockDepth = 0) const;

    // returns true if block exist in BlockTree
    bool hasBlock(const Hash& blockHash) const;

    // returns true if block is in last level of BlockTree
    bool isInLastLevel(const Hash& blockHash) const;

    // bans block (his hash)
    void banBlock(const Hash& hash, const std::string& reason);

    // check if block is baned
    bool isBanned(const Hash& blockHash, const MinerId& minerId) const;

    // returns levels
    std::deque<std::map<Hash, BlockTreeNode>> getLevels();

    // returns depth of block in level 0
    uint32_t getBaseDepth() const;

    // returns debug info
    json getDebugInfo() const;

protected:
    MinerId getExpectedMinerId(const Hash& parentHash, size_t skippedBlocks) const;
    std::deque<BlockTreeNode> getParents(const Hash& blockHeaderHash, bool withoutPendingBlocks = false,
                                         bool includeItself = false, bool includeFirstLevel = true) const;

    void onPendingBlockUpdated(const PendingBlock_ptr& pendingBlock);
    bool finishPendingBlock(BlockTreeNode& node);
    void clearPendingBlock(BlockTreeNode& node);
    void cleanup();

private:
    const std::shared_ptr<Bans> m_bans;
    const std::shared_ptr<PendingTransactions> m_pendingTransactions;
    mutable std::recursive_mutex m_mutex;
    mutable Logger m_logger;
    mutable logging::attributes::mutable_constant<std::string> m_loggerId;

    MinersQueue m_miningQueue;
    std::deque<std::map<Hash, BlockTreeNode>> m_levels; // level 0 is root, level size()-1 is a pending header
    std::set<Hash> m_bannedBlocks;
    std::set<MinerId> m_bannedReporters;
};

}
