#pragma once

namespace logpass {

class BlockBody;
using BlockBody_ptr = std::shared_ptr<BlockBody>;
using BlockBody_cptr = std::shared_ptr<const BlockBody>;

class BlockBody {
public:
    BlockBody() = default;
    BlockBody(size_t transactions, size_t transactionsSize, const std::vector<Hash>& hashes);
    BlockBody(const BlockBody&) = delete;
    BlockBody& operator = (const BlockBody&) = delete;

    void serialize(Serializer& s);

    uint8_t getVersion() const
    {
        return m_version;
    }

    uint8_t getMaxVersion() const
    {
        return m_maxVersion;
    }

    uint32_t getTransactions() const
    {
        return m_transactions;
    }

    uint32_t getTransactionsSize() const
    {
        return m_transactionsSize;
    }

    const std::vector<Hash>& getHashes() const
    {
        return m_hashes;
    }

    const Hash& getHash() const
    {
        return m_hash;
    }

    void toJSON(json& j) const;

private:
    uint8_t m_version = 1;
    uint8_t m_maxVersion = kVersionMajor; // maximum node version supported by miner
    uint32_t m_transactions = 0;
    uint32_t m_transactionsSize = 0;
    std::vector<Hash> m_hashes;

    // hash is calculated during object creation
    Hash m_hash;
};

}
