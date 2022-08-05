#include "pch.h"
#include "block_transaction_ids.h"

namespace logpass {

BlockTransactionIds::BlockTransactionIds(const std::vector<TransactionId>& transactionIds) : m_transactionIds(transactionIds)
{
    Serializer s;
    serialize(s); // calculate hash
}

void BlockTransactionIds::serialize(Serializer& s)
{
    size_t serializerPos = s.pos();
    s(m_transactionIds);
    if (m_transactionIds.size() == 0 || m_transactionIds.size() > CHUNK_SIZE)
        THROW_SERIALIZER_EXCEPTION("Invalid block transaction ids size");

    // calculate hash
    if (!m_hash.isValid()) {
        m_hash = Hash(s.begin() + serializerPos, s.pos() - serializerPos);
    }
}

void BlockTransactionIds::toJSON(json& j) const
{
    j["transaction_ids"] = m_transactionIds;
    j["hash"] = m_hash;
}

}
