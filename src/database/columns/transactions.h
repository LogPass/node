#pragma once

#include "stateful_column.h"
#include <blockchain/transactions/transaction.h>

namespace logpass {
namespace database {

struct TransactionsColumnState : public ColumnState {
    uint64_t transactionsCount = 0;
    uint64_t transactionsSize = 0;
    std::map<uint16_t, uint64_t> transactionsCountByType;

    void serialize(Serializer& s)
    {
        ColumnState::serialize(s);
        s(transactionsCount);
        s(transactionsSize);
        s(transactionsCountByType);
    }
};

// keeps transactions
class TransactionsColumn : public StatefulColumn<TransactionsColumnState> {
public:
    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return "transactions";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    std::pair<Transaction_cptr, uint32_t> getTransaction(const TransactionId& transactionId, bool confirmed) const;
    std::map<TransactionId, Transaction_cptr> getTransactions(const std::vector<TransactionId>& transactionIds);
    void addTransaction(const Transaction_cptr& transaction, uint32_t blockId);

    uint64_t getTransactionsCount(bool confirmed) const;
    uint64_t getTransactionsSize(bool confirmed) const;

    uint64_t getTransactionsCountByType(uint16_t transactionType, bool confirmed) const;

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    std::map<TransactionId, std::pair<Transaction_cptr, uint32_t>> m_transactions;
};

}
}
