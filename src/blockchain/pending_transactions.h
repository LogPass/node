#pragma once

#include <database/database.h>

#include "events.h"
#include "block/pending_block.h"

namespace logpass {

// takes care od pending transactions, all functions are thread-safe
class PendingTransactions {
    struct PendingTransactionEntry {
        Transaction_cptr transaction;
        uint32_t index = 0;
        MinerId reporter;
        bool isCryptoVerified = false;
        bool wasExecuted = false;
    };

public:
    PendingTransactions() = default;
    PendingTransactions(const PendingTransactions&) = delete;
    PendingTransactions& operator=(const PendingTransactions&) = delete;

    // checks if there's space for given transaction
    bool canAddTransaction(const TransactionId& transactionId) const;

    // adds transaction, transaction is always added, returns true if transaction is new
    bool addTransaction(const Transaction_cptr& transaction, const MinerId& reporter);

    // adds transactions, transactions are always added, returns number of new transactions
    size_t addTransactions(const std::vector<Transaction_cptr>& transactions, const MinerId& reporter);

    // adds transaction as executed, transaction is always added
    bool addExecutedTransaction(const Transaction_cptr& transaction);

    // adds transactions as executed, transaction is always added
    size_t addExecutedTransactions(const std::vector<Transaction_cptr>& transactions);

    // adds transaction if it's requested
    bool addTransactionIfRequested(const Transaction_cptr& transaction, bool addToPendingBlockOnly = false);

    // returns pending transaction if exists
    Transaction_cptr getTransaction(const TransactionId& transactionId) const;

    // return transactions
    std::map<TransactionId, Transaction_cptr> getTransactions(const std::set<TransactionId>& transactionIds) const;

    // checks if transaction with given id is executed
    bool hasExecutedTransaction(const TransactionId& transactionId) const;

    // checks if transactions with given ids exist
    std::set<TransactionId> hasTransactions(const std::set<TransactionId>& transactionIds) const;

    // check if transaction with given id was crypto verified
    bool isTransactionCryptoVerified(const TransactionId& transactionId) const;

    // sets transaction with given id as crypto verified
    void setTransactionAsCryptoVerified(const TransactionId& transactionId);

    // adds pending block
    void addPendingBlock(const PendingBlock_ptr& pendingBlock);

    // removes pending block
    void removePendingBlock(const PendingBlock_ptr& pendingBlock);

    // returns vector of unexuted transactions sorted by priority of execution
    std::vector<Transaction_cptr> getPendingTransactions(size_t limit) const;

    // returns vector of executed transactions sorted by order of execution
    std::vector<Transaction_cptr> getExecutedTransactions(size_t limit) const;

    // updates executed transactions, boolean in pair says if transaction was executed correctly or not
    // not executed transactions are removed form pending transactions
    // should be used only by blockchain main execution thread
    void updateTransactions(const std::vector<std::pair<TransactionId, bool>>& transactions);

    // removes pending (not executed) transactions
    // should be used only by blockchain main execution thread
    void removeTransactions(const std::set<TransactionId>& transactionIds);

    // moves all executed transactions to pending transactions
    // should be used only by blockchain main execution thread
    void clearExecutedTransactions();

    // returns count of transactions
    uint32_t getTransactionsCount() const;

    // returns size of transactions
    uint32_t getTransactionsSize() const;

    // returns count of executed pending transactions
    uint32_t getPendingTransactionsCount() const;

    // returns size of executed pending transactions
    uint32_t getPendingTransactionsSize() const;

    // returns count of executed pending transactions
    uint32_t getExecutedTransactionsCount() const;

    // returns size of executed pending transactions
    uint32_t getExecutedTransactionsSize() const;

    // returns number of requested transactions
    uint32_t getRequestedTransactionsCount() const;

    // returns max count of pending transactions (can be exceeded in some cases)
    uint32_t getMaxPendingTransactionsCount() const;

    // returns max size of pending transactions (can be exceeded in some cases)
    uint32_t getMaxPendingTransactionsSize() const;

    // returns json debug info
    json getDebugInfo() const;

private:
    mutable std::shared_mutex m_mutex;
    std::map<TransactionId, PendingTransactionEntry> m_executedTransactions;
    std::map<TransactionId, PendingTransactionEntry> m_pendingTransactions;
    std::map<TransactionId, std::set<PendingBlock_ptr>> m_requestedTransactions;
    std::deque<Transaction_cptr> m_pendingTransactionsQueue;
    uint32_t m_executedTransactionsSize = 0;
    uint32_t m_transactionsSize = 0;
};

}
