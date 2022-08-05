#pragma once

#include "facade.h"

#include <blockchain/block/block.h>
#include <database/columns/blocks.h>
#include <database/columns/transactions.h>

namespace logpass {
namespace database {

class BlocksFacade : public Facade {
public:
    BlocksFacade(BlocksColumn* blocks, TransactionsColumn* transactions, bool confirmed) :
        m_blocks(blocks), m_transactions(transactions), m_confirmed(confirmed) {}

    BlocksFacade(const std::unique_ptr<BlocksColumn>& blocks, const std::unique_ptr<TransactionsColumn>& transactions,
                 bool confirmed) :
        BlocksFacade(blocks.get(), transactions.get(), confirmed)
    {}

    // returns block, may be nullptr
    Block_cptr getBlock(uint32_t blockId) const;
    // returns block header with given id, may return nullptr
    BlockHeader_cptr getBlockHeader(uint32_t blockId) const;
    // returns block header after block with given id, may return nullptr
    BlockHeader_cptr getNextBlockHeader(uint32_t blockId) const;
    // returns block body with given id, may return nullptr
    BlockBody_cptr getBlockBody(uint32_t blockId) const;
    // returns block transaction ids with given id and chunk, may return nullptr
    BlockTransactionIds_cptr getBlockTransactionIds(uint32_t blockId, uint8_t chunk) const;

    // returns latest block headers and bodies 
    std::map<uint32_t, std::pair<BlockHeader_cptr, BlockBody_cptr>> getLatestBlocks() const;
    // returns latest block header
    BlockHeader_cptr getLatestBlockHeader() const;
    // returns id of latest block
    uint32_t getLatestBlockId() const;

    // returns mining queue
    MinersQueue getMinersQueue() const;

    // adds block
    void addBlock(const Block_cptr& block);

private:
    BlocksColumn* m_blocks;
    TransactionsColumn* m_transactions;
    bool m_confirmed;
};

}
}
