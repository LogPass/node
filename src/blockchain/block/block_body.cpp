#include "pch.h"
#include "block_body.h"
#include "block_transaction_ids.h"

namespace logpass {

BlockBody::BlockBody(size_t transactions, size_t transactionsSize, const std::vector<Hash>& hashes) :
    m_transactions(transactions), m_transactionsSize(transactionsSize), m_hashes(hashes)
{
    // calculate hash
    Serializer s;
    serialize(s);
    ASSERT(m_hash.isValid());
}

void BlockBody::serialize(Serializer& s)
{
    size_t serializerPos = s.pos();

    s(m_version);
    if (m_version != 1) {
        THROW_SERIALIZER_EXCEPTION("Invalid block body version");
    }
    s(m_maxVersion);
    if (m_maxVersion < m_version) {
        THROW_SERIALIZER_EXCEPTION("Invalid block body max version");
    }

    s(m_transactions);
    if (m_transactions > kBlockMaxTransactions) {
        THROW_SERIALIZER_EXCEPTION("Too many transactions in block body");
    }
    s(m_transactionsSize);
    if (m_transactions > kBlockMaxTransactionsSize) {
        THROW_SERIALIZER_EXCEPTION("Too big size of transactions in block body");
    }

    s.serialize<uint8_t>(m_hashes);

    if (m_hashes.size() != (m_transactions + BlockTransactionIds::CHUNK_SIZE - 1) / BlockTransactionIds::CHUNK_SIZE) {
        THROW_SERIALIZER_EXCEPTION("Invalid number of block body hashes");
    }

    // calculate hash
    if (!m_hash.isValid()) {
        m_hash = Hash(s.begin() + serializerPos, s.pos() - serializerPos);
    }
}

void BlockBody::toJSON(json& j) const
{
    j["version"] = m_version;
    j["max_version"] = m_maxVersion;
    j["transactions_count"] = m_transactions;
    j["transactions_size"] = m_transactionsSize;
    j["transaction_ids_hashes"] = m_hashes;
    j["hash"] = m_hash;
}

}
