#pragma once

namespace logpass {

class BlockTransactionIds;
using BlockTransactionIds_ptr = std::shared_ptr<BlockTransactionIds>;
using BlockTransactionIds_cptr = std::shared_ptr<const BlockTransactionIds>;

class BlockTransactionIds {
public:
    static const size_t CHUNK_SIZE = kBlockTransactionsPerChunk;

    BlockTransactionIds() = default;
    BlockTransactionIds(const std::vector<TransactionId>& transactionIds);
    BlockTransactionIds(const BlockTransactionIds&) = delete;
    BlockTransactionIds& operator = (const BlockTransactionIds&) = delete;

    void serialize(Serializer& s);

    Hash getHash() const
    {
        return m_hash;
    }

    std::vector<TransactionId>::const_iterator begin() const
    {
        return m_transactionIds.begin();
    }

    std::vector<TransactionId>::const_iterator end() const
    {
        return m_transactionIds.end();
    }

    const TransactionId& at(size_t index) const
    {
        ASSERT(index < m_transactionIds.size());
        return m_transactionIds[index];
    }

    size_t size() const
    {
        return m_transactionIds.size();
    }

    void toJSON(json& j) const;

private:
    std::vector<TransactionId> m_transactionIds;

    // hash is calculated during object creation
    Hash m_hash;
};

}
