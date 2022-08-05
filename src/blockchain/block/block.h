#pragma once

#include "block_header.h"
#include "block_body.h"
#include "block_transaction_ids.h"
#include "block_iterator.h"

#include <blockchain/transactions/transaction.h>

namespace logpass {

class Block;
using Block_ptr = std::shared_ptr<Block>;
using Block_cptr = std::shared_ptr<const Block>;

class Block {
    friend class BlockIterator;

public:
    Block();
    Block(size_t id);
    Block(const BlockHeader_cptr& header, const BlockBody_cptr& body,
          const std::vector<BlockTransactionIds_cptr>& transactionIds,
          const std::map<TransactionId, Transaction_cptr>& transactions);

    Block(const Block&) = delete;
    Block& operator = (const Block&) = delete;

    // creates and signs new block
    static Block_cptr create(uint32_t id, uint32_t depth, const MinersQueue& nextMiners,
                             const std::vector<Transaction_cptr>& transactions, const Hash& prevBlockHash,
                             const PrivateKey& privateKey);

    // serializes block
    void serialize(Serializer& s);

    // validates block stucture, hashes and signature
    bool validate(const PublicKey& minerKey, const Hash& prevBlockHeaderHash) const;

    // returns id of block
    uint32_t getId() const
    {
        return m_header->getId();
    }

    uint32_t getDepth() const
    {
        return m_header->getDepth();
    }

    MinersQueue getNextMiners() const
    {
        return m_header->getNextMiners();
    }

    MinerId getMinerId() const
    {
        return m_header->getMiner();
    }

    uint8_t getSkippedBlocks() const
    {
        return m_header->getSkippedBlocks();
    }

    Hash getHeaderHash() const
    {
        return m_header->getHash();
    }

    //
    Hash getPrevHeaderHash() const
    {
        return m_header->getPrevHeaderHash();
    }

    Hash getBodyHash() const
    {
        return m_header->getBodyHash();
    }

    // return number of transactions in block
    size_t getTransactions() const
    {
        return m_body->getTransactions();
    }

    size_t getTransactionsSize() const
    {
        return m_body->getTransactionsSize();
    }

    BlockHeader_cptr getBlockHeader() const
    {
        return m_header;
    }

    BlockBody_cptr getBlockBody() const
    {
        return m_body;
    }

    // return block hashes of transactions
    std::vector<BlockTransactionIds_cptr> getBlockTransactionIds() const
    {
        return m_transactionIds;
    }

    // return hash and size of transaction with given index
    TransactionId getTransactionId(size_t index) const;
    // return transaction with given hash
    Transaction_cptr getTransaction(const TransactionId& transactionId) const;

    // iterate over transactions with correct order
    BlockIterator begin() const
    {
        return BlockIterator(*this, 0);
    }

    BlockIterator end() const
    {
        return BlockIterator(*this, getTransactions());
    }

    std::string toString() const
    {
        return m_header->toString();
    }

private:
    BlockHeader_cptr m_header;
    BlockBody_cptr m_body;
    std::vector<BlockTransactionIds_cptr> m_transactionIds;
    std::map<TransactionId, Transaction_cptr> m_transactions;
};

}
