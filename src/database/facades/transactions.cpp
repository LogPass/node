#include "pch.h"
#include "transactions.h"

namespace logpass {
namespace database {

Transaction_cptr TransactionsFacade::getTransaction(const TransactionId& transactionId) const
{
    return m_transactions->getTransaction(transactionId, m_confirmed).first;
}

std::pair<Transaction_cptr, uint32_t> TransactionsFacade::getTransactionWithBlockId(
    const TransactionId& transactionId) const
{
    return m_transactions->getTransaction(transactionId, m_confirmed);
}

bool TransactionsFacade::hasTransaction(const TransactionId& transactionId) const
{
    return m_transactions->getTransaction(transactionId, m_confirmed).first != nullptr;
}

bool TransactionsFacade::hasTransactionHash(uint32_t transactionBlockId, const Hash& hash) const
{
    return m_transactionBodies->hasTransactionHash(transactionBlockId, hash, m_confirmed);
}

void TransactionsFacade::addTransaction(const Transaction_cptr& transaction, uint32_t blockId)
{
    m_transactions->addTransaction(transaction, blockId);
    m_transactionBodies->addTransactionHashHash(transaction->getBlockId(),
                                                        transaction->getDuplicationHash());
}

uint64_t TransactionsFacade::getTransactionsCount() const
{
    return m_transactions->getTransactionsCount(m_confirmed);
}

uint64_t TransactionsFacade::getTransactionsSize() const
{
    return m_transactions->getTransactionsSize(m_confirmed);
}

uint64_t TransactionsFacade::getTransactionsCountByType(uint16_t transactionType) const
{
    return m_transactions->getTransactionsCountByType(transactionType, m_confirmed);
}

uint64_t TransactionsFacade::getNewTransactionsCount() const
{
    return getTransactionsCount() - m_transactions->getTransactionsCount(true);
}

uint64_t TransactionsFacade::getNewTransactionsSize() const
{
    return getTransactionsSize() - m_transactions->getTransactionsSize(true);
}

uint64_t TransactionsFacade::getNewTransactionsCountByType(uint16_t transactionType) const
{
    return getTransactionsCountByType(transactionType) -
        m_transactions->getTransactionsCountByType(transactionType, true);
}

}
}
