#include "pch.h"
#include "block.h"
#include "block_iterator.h"

namespace logpass {

BlockIterator::BlockIterator(const Block& block, size_t index) : m_block(block), m_index(index)
{
    ASSERT(m_index <= m_block.getTransactions());
}

Transaction_cptr BlockIterator::operator*() const
{
    ASSERT(m_index < m_block.getTransactions());
    TransactionId transactionId = m_block.getTransactionId(m_index);
    Transaction_cptr transaction = m_block.getTransaction(transactionId);
    ASSERT(transaction);
    return transaction;
}

bool BlockIterator::operator==(const BlockIterator& second) const
{
    return m_index == second.m_index;
}

bool BlockIterator::operator!=(const BlockIterator& second) const
{
    return m_index != second.m_index;
}

BlockIterator& BlockIterator::operator++()
{
    if (m_index < m_block.getTransactions()) {
        m_index += 1;
    }
    return *this;
}

}
