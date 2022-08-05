#include "pch.h"
#include "transactions.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions TransactionsColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    return columnOptions;
}

std::pair<Transaction_cptr, uint32_t> TransactionsColumn::getTransaction(const TransactionId& transactionId,
                                                                         bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_transactions.find(transactionId);
        if (it != m_transactions.end())
            return { it->second.first, 0 };
    }

    Serializer_ptr s = get<TransactionId>(transactionId);
    if (!s)
        return { nullptr, 0 };
    uint32_t blockId = s->get<uint32_t>();
    return { Transaction::load(*s), blockId };
}

std::map<TransactionId, Transaction_cptr> TransactionsColumn::getTransactions(
    const std::vector<TransactionId>& transactionIds)
{
    std::map<TransactionId, Transaction_cptr> transactions;
    auto results = multiGet(transactionIds);
    for (size_t i = 0; i < results.size(); ++i) {
        if (!results[i]) {
            transactions.emplace(transactionIds[i], nullptr);
        } else {
            results[i]->get<uint32_t>(); // block id
            transactions.emplace(transactionIds[i], Transaction::load(*results[i]));
        }
    }
    return transactions;
}

void TransactionsColumn::addTransaction(const Transaction_cptr& transaction, uint32_t blockId)
{
    ASSERT(!getTransaction(transaction->getId(), false).first);
    std::unique_lock lock(m_mutex);
    m_transactions[transaction->getId()] = { transaction, blockId };
    state().transactionsCount += 1;
    state().transactionsSize += transaction->getSize();
    state().transactionsCountByType.emplace(transaction->getType(), 0).first->second += 1;
}

uint64_t TransactionsColumn::getTransactionsCount(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).transactionsCount;
}

uint64_t TransactionsColumn::getTransactionsSize(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).transactionsSize;
}

uint64_t TransactionsColumn::getTransactionsCountByType(uint16_t transactionType, bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    auto& transactionsCountByType = state(confirmed).transactionsCountByType;
    auto it = transactionsCountByType.find(transactionType);
    if (it == transactionsCountByType.end())
        return 0;
    return it->second;
}

void TransactionsColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_transactions.empty());
    StatefulColumn::load();
}

void TransactionsColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [transactionId, transactionPair] : m_transactions) {
        Serializer sVal;
        sVal.put<uint32_t>(transactionPair.second);
        sVal(transactionPair.first);
        put<TransactionId>(batch, transactionId, sVal);
    }
}

void TransactionsColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_transactions.clear();
}

void TransactionsColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_transactions.clear();
}

}
}
