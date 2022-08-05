#pragma once

namespace logpass {

// not thread-safe
class SharedTransactionIds {
public:
    SharedTransactionIds(size_t chunks, size_t chunkCapacity) :
        m_maxChunks(chunks), m_chunkCapacity(chunkCapacity)
    {
        m_chunks.emplace_front();
    }

    // returns true when transaction id is new
    bool addTransactionId(const TransactionId& transactionId)
    {
        if (hasTransactionId(transactionId)) {
            return false;
        }

        auto& chunk = m_chunks.front();
        chunk.insert(transactionId);
        if (chunk.size() != m_chunkCapacity) {
            return true;
        }

        m_chunks.emplace_front();
        if (m_chunks.size() > m_maxChunks) {
            m_chunks.pop_back();
        }
        return true;
    }

    bool hasTransactionId(const TransactionId& transactionId)
    {
        for (auto& chunk : m_chunks) {
            if (chunk.contains(transactionId)) {
                return true;
            }
        }
        return false;
    }

private:
    const size_t m_maxChunks;
    const size_t m_chunkCapacity;
    std::deque<std::set<TransactionId>> m_chunks;
};

}
