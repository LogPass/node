#pragma once

#include "stateful_column.h"
#include <blockchain/transactions/transaction.h>

namespace logpass {
namespace database {

struct TransactionHashesColumnState : public ColumnState {

};

// keeps transaction body hashes
class TransactionHashesColumn : public StatefulColumn<TransactionHashesColumnState> {
public:
    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return "transaction_hashes";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    bool hasTransactionHash(uint32_t transactionBlockId, const Hash& hash, bool confirmed) const;
    void addTransactionHashHash(uint32_t transactionBlockId, const Hash& hash);

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    std::map<uint32_t, std::set<Hash>> m_hashes;
};

}
}
