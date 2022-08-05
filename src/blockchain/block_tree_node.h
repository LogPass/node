#pragma once

#include "block/block.h"
#include "block/pending_block.h"

namespace logpass {

// not thread-safe
struct BlockTreeNode {
    BlockTreeNode(const Block_cptr& block, const PendingBlock_ptr& pendingBlock, bool executed, MinerId reporter,
                  MinerId miner) :
        block(block), pendingBlock(pendingBlock), executed(executed), reporter(reporter), miner(miner)
    {
        ASSERT(!block != !pendingBlock);
    }

    Block_cptr block;
    PendingBlock_ptr pendingBlock;
    bool executed;
    MinerId reporter;
    MinerId miner;
    bool hasLockedTransactions = false;

    uint32_t getId() const
    {
        if (block) {
            return block->getId();
        }
        return pendingBlock->getId();
    }

    uint32_t getDepth() const
    {
        if (block) {
            return block->getDepth();
        }
        return pendingBlock->getDepth();
    }

    MinersQueue getNextMiners() const
    {
        if (block) {
            return block->getNextMiners();
        }
        return pendingBlock->getNextMiners();
    }

    uint8_t getSkippedBlocks() const
    {
        if (block) {
            return block->getSkippedBlocks();
        }
        return pendingBlock->getSkippedBlocks();
    }

    Hash getHeaderHash() const
    {
        if (block) {
            return block->getHeaderHash();
        }
        return pendingBlock->getHeaderHash();
    }

    Hash getPrevHeaderHash() const
    {
        if (block) {
            return block->getPrevHeaderHash();
        }
        return pendingBlock->getPrevHeaderHash();
    }

    MinerId getMinerId() const
    {
        if (block) {
            return block->getMinerId();
        }
        return pendingBlock->getMinerId();
    }

    std::string toString() const
    {
        if (block) {
            return block->toString();
        }
        return pendingBlock->toString();
    }
};

}
