#pragma once

#include "facade.h"

#include <blockchain/transactions/transaction.h>
#include <database/columns/user_history.h>
#include <database/columns/users.h>
#include <database/columns/transactions.h>
#include <database/columns/transaction_hashes.h>

namespace logpass {
namespace database {

class TransactionsFacade : public Facade {
public:
    TransactionsFacade(TransactionsColumn* transactions, TransactionHashesColumn* transactionBodies, bool confirmed) :
        m_transactions(transactions), m_transactionBodies(transactionBodies), m_confirmed(confirmed)
    {}

    TransactionsFacade(const std::unique_ptr<TransactionsColumn>& transactions,
                       const std::unique_ptr<TransactionHashesColumn>& transactionBodies, bool confirmed) :
        TransactionsFacade(transactions.get(), transactionBodies.get(), confirmed)
    {}

    Transaction_cptr getTransaction(const TransactionId& transactionId) const;
    std::pair<Transaction_cptr, uint32_t> getTransactionWithBlockId(const TransactionId& transactionId) const;

    bool hasTransaction(const TransactionId& transactionId) const;

    bool hasTransactionHash(uint32_t transactionBlockId, const Hash& hash) const;

    void addTransaction(const Transaction_cptr& transaction, uint32_t blockId);

    uint64_t getTransactionsCount() const;
    uint64_t getTransactionsSize() const;
    uint64_t getTransactionsCountByType(uint16_t transactionType) const;

    uint64_t getNewTransactionsCount() const;
    uint64_t getNewTransactionsSize() const;
    uint64_t getNewTransactionsCountByType(uint16_t transactionType) const;

private:
    TransactionsColumn* m_transactions;
    TransactionHashesColumn* m_transactionBodies;
    bool m_confirmed;
};

}
}
