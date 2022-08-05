#include "pch.h"
#include "blocks.h"

namespace logpass {
namespace database {

Block_cptr BlocksFacade::getBlock(uint32_t blockId) const
{
    auto header = m_blocks->getBlockHeader(blockId, m_confirmed);
    if (!header) {
        return nullptr;
    }
    auto body = m_blocks->getBlockBody(blockId, m_confirmed);
    if (!body || header->getBodyHash() != body->getHash()) {
        return nullptr;
    }
    std::vector<BlockTransactionIds_cptr> blockTransactionIds;
    std::vector<Hash> transactionIdsHashes = body->getHashes();
    for (size_t index = 0; Hash& hash : transactionIdsHashes) {
        auto transactionIds = m_blocks->getBlockTransactionIds(blockId, index++, m_confirmed);
        if (!transactionIds || transactionIds->getHash() != hash) {
            return nullptr;
        }
        blockTransactionIds.push_back(transactionIds);
    }
    std::vector<TransactionId> transactionIds;
    transactionIds.reserve(body->getTransactions());
    for (auto& chunk : blockTransactionIds) {
        transactionIds.insert(transactionIds.end(), chunk->begin(), chunk->end());
    }
    std::map<TransactionId, Transaction_cptr> transactions = m_transactions->getTransactions(transactionIds);
    return std::make_shared<Block>(header, body, blockTransactionIds, transactions);
}

BlockHeader_cptr BlocksFacade::getBlockHeader(uint32_t blockId) const
{
    return m_blocks->getBlockHeader(blockId, m_confirmed);
}

BlockHeader_cptr BlocksFacade::getNextBlockHeader(uint32_t blockId) const
{
    return m_blocks->getNextBlockHeader(blockId, m_confirmed);
}

BlockBody_cptr BlocksFacade::getBlockBody(uint32_t blockId) const
{
    return m_blocks->getBlockBody(blockId, m_confirmed);
}

BlockTransactionIds_cptr BlocksFacade::getBlockTransactionIds(uint32_t blockId, uint8_t chunk) const
{
    return m_blocks->getBlockTransactionIds(blockId, chunk, m_confirmed);
}

std::map<uint32_t, std::pair<BlockHeader_cptr, BlockBody_cptr>> BlocksFacade::getLatestBlocks() const
{
    return m_blocks->getLatestBlocks(m_confirmed);
}

BlockHeader_cptr BlocksFacade::getLatestBlockHeader() const
{
    return m_blocks->getLatestBlockHeader(m_confirmed);
}

uint32_t BlocksFacade::getLatestBlockId() const
{
    auto latestBlockHeader = getLatestBlockHeader();
    if (!latestBlockHeader) {
        return 0;
    }
    return latestBlockHeader->getId();
}

MinersQueue BlocksFacade::getMinersQueue() const
{
    return m_blocks->getMinersQueue(m_confirmed);
}

void BlocksFacade::addBlock(const Block_cptr& block)
{
    m_blocks->addBlock(block);
#ifndef NDEBUG
    // make sure all transaction are added
    ASSERT(m_transactions->getTransactionsCount(false) - m_transactions->getTransactionsCount(true) ==
           block->getBlockBody()->getTransactions());
    for (auto transaction : *block) {
        ASSERT(m_transactions->getTransaction(transaction->getId(), false).first);
    }
#endif
}

}
}
