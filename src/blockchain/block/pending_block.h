#pragma once

#include "block.h"

namespace logpass {

class PendingBlock;
using PendingBlock_ptr = std::shared_ptr<PendingBlock>;
using PendingBlock_cptr = std::shared_ptr<const PendingBlock>;

// class used to build block over time from it's parts, thread-safe
class PendingBlock : public std::enable_shared_from_this<PendingBlock> {
public:
    enum class Status : uint8_t {
        MISSING_BODY,
        MISSING_TRANSACTION_IDS,
        MISSING_TRANSACTIONS,
        COMPLETE,
        FINISHED,
        EXPIRED,
        INVALID
    };

    enum class AddResult : uint8_t {
        CORRECT, // block has been updated
        DUPLICATED, // block already has that data
        INVALID_DATA, // new block part is invalid
        INVALID_BLOCK // new block part is valid, but block is invalid
    };

    PendingBlock(const BlockHeader_cptr& header, const MinerId& minerId,
                 const std::function<void(PendingBlock_ptr)>& onUpdated)
        : m_header(header), m_minerId(minerId), m_onUpdated(onUpdated)
    {
        ASSERT(m_header && m_onUpdated);
    }

    PendingBlock(const PendingBlock&) = delete;
    PendingBlock& operator = (const PendingBlock&) = delete;

    // returns id of block
    uint32_t getId() const
    {
        return m_header->getId();
    }

    // returns depth of block
    uint32_t getDepth() const
    {
        return m_header->getDepth();
    }

    // returns next miners of the block
    MinersQueue getNextMiners() const
    {
        return m_header->getNextMiners();
    }

    // returns number of skipped blocks
    uint8_t getSkippedBlocks() const
    {
        return m_header->getSkippedBlocks();
    }

    // returns hash of header
    Hash getHeaderHash() const
    {
        return m_header->getHash();
    }

    // returns hash of previous header
    Hash getPrevHeaderHash() const
    {
        return m_header->getPrevHeaderHash();
    }

    MinerId getMinerId() const
    {
        return m_minerId;
    }

    std::string toString() const
    {
        return m_header->toString() + " ("s + std::to_string(m_transactions.size()) + "/"s +
            std::to_string(m_transactions.size() + m_missingTransactions.size()) + ")"s;
    }

    // returns what's missing in pending block
    Status getStatus() const;

    // returns hash of body part
    Hash getBlockBodyHash() const;

    // adds block body
    AddResult addBlockBody(const BlockBody_cptr& blockBody);

    // returns indexes and hash of missing transactionIds
    std::vector<std::pair<uint32_t, Hash>> getMissingTransactionIdsHashes(uint32_t limit = 0) const;

    // adds block transaction ids
    AddResult addBlockTransactionIds(const std::vector<BlockTransactionIds_cptr>& blockTransactionIds);
    AddResult addBlockTransactionIds(const BlockTransactionIds_cptr& blockTransactionIds)
    {
        return addBlockTransactionIds(std::vector<BlockTransactionIds_cptr>{ blockTransactionIds });
    }

    // returns ids of all transactions, but only when all transactionIds parts are loaded
    std::set<TransactionId> getTransactionIds() const;

    // gets missing transaction ids
    std::set<TransactionId> getMissingTransactionIds(uint32_t countLimit = 0, uint32_t sizeLimit = 0) const;

    // adds multiple transactions
    AddResult addTransactions(const std::vector<Transaction_cptr>& transactions, bool executeCallback = true);

    // adds single transaction
    AddResult addTransaction(const Transaction_cptr& transaction, bool executeCallback = true)
    {
        return addTransactions({ transaction }, executeCallback);
    }

    // check if block has transaction
    bool hasTransaction(const TransactionId& transactionId) const;

    // sets block as invalid
    void setInvalid();

    // checks if block is invalid
    bool isInvalid();

    // sets block as expired
    void setExpired();

    // check if block is expired
    bool isExpired();

    // sets block as finished
    void setFinished();

    // check if block is finished
    bool isFinished();

    // when all the data is collected, it creates full block
    Block_cptr createBlock() const;

private:
    const BlockHeader_cptr m_header;
    const MinerId m_minerId;
    const std::function<void(PendingBlock_ptr)> m_onUpdated;
    mutable std::shared_mutex m_mutex;

    BlockBody_cptr m_body;
    std::vector<BlockTransactionIds_cptr> m_transactionIds;
    std::map<TransactionId, Transaction_cptr> m_transactions;
    std::set<TransactionId> m_missingTransactions;
    bool m_invalid = false;
    bool m_expired = false;
    bool m_finished = false;
};

}
