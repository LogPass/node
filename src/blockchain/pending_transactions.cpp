#include "pch.h"

#include "pending_transactions.h"

namespace logpass {

bool PendingTransactions::canAddTransaction(const TransactionId& transactionId) const
{
    std::shared_lock lock(m_mutex);
    if (m_requestedTransactions.count(transactionId) != 0) {
        return true;
    }
    if (m_pendingTransactions.size() + m_executedTransactions.size() >= getMaxPendingTransactionsCount()) {
        return false;
    }
    if (transactionId.getSize() + m_transactionsSize > getMaxPendingTransactionsSize()) {
        return false;
    }
    return true;
}

bool PendingTransactions::addTransaction(const Transaction_cptr& transaction, const MinerId& reporter)
{
    return addTransactions({ transaction }, reporter) == 1;
}

size_t PendingTransactions::addTransactions(const std::vector<Transaction_cptr>& transactions, const MinerId& reporter)
{
    std::unique_lock lock(m_mutex);
    size_t addedTransactions = 0;
    std::set<PendingBlock_ptr> pendingBlocks;
    for (auto& transaction : transactions) {
        auto requestedTransactionsIterator = m_requestedTransactions.find(transaction->getId());
        if (requestedTransactionsIterator != m_requestedTransactions.end()) {
            std::set<PendingBlock_ptr> transactionPendingBlocks = requestedTransactionsIterator->second;
            pendingBlocks.insert(transactionPendingBlocks.begin(), transactionPendingBlocks.end());
            m_requestedTransactions.erase(requestedTransactionsIterator);
        }

        if (m_executedTransactions.contains(transaction->getId()))
            continue;

        if (m_pendingTransactions.contains(transaction->getId()))
            continue;

        m_pendingTransactions.emplace(transaction->getId(),
                                      PendingTransactionEntry{
                                          .transaction = transaction,
                                          .reporter = reporter
                                      });
        m_pendingTransactionsQueue.push_back(transaction);
        m_transactionsSize += transaction->getSize();
        addedTransactions += 1;
    }

    // update pending blocks
    lock.unlock();
    for (auto& pendingBlock : pendingBlocks) {
        pendingBlock->addTransactions(transactions);
    }
    return addedTransactions;
}

bool PendingTransactions::addExecutedTransaction(const Transaction_cptr& transaction)
{
    return addExecutedTransactions({ transaction }) == 1;
}

size_t PendingTransactions::addExecutedTransactions(const std::vector<Transaction_cptr>& transactions)
{
    std::unique_lock lock(m_mutex);
    size_t addedTransactions = 0;
    std::set<PendingBlock_ptr> pendingBlocks;
    for (auto& transaction : transactions) {
        auto requestedTransactionsIterator = m_requestedTransactions.find(transaction->getId());
        if (requestedTransactionsIterator != m_requestedTransactions.end()) {
            std::set<PendingBlock_ptr> transactionPendingBlocks = requestedTransactionsIterator->second;
            pendingBlocks.insert(transactionPendingBlocks.begin(), transactionPendingBlocks.end());
            m_requestedTransactions.erase(requestedTransactionsIterator);
        }

        if (m_executedTransactions.contains(transaction->getId())) {
            continue;
        }

        auto pendingTransactionIt = m_pendingTransactions.find(transaction->getId());
        if (pendingTransactionIt != m_pendingTransactions.end()) {
            auto transactionsQueueIt = std::find_if(m_pendingTransactionsQueue.begin(),
                                                    m_pendingTransactionsQueue.end(),
                                                    [&](const Transaction_cptr& entry) {
                return transaction->getId() == entry->getId();
            });
            ASSERT(transactionsQueueIt != m_pendingTransactionsQueue.end());
            m_pendingTransactionsQueue.erase(transactionsQueueIt);
            m_pendingTransactions.erase(pendingTransactionIt);
        } else {
            m_transactionsSize += transaction->getSize();
        }

        m_executedTransactions.emplace(transaction->getId(), PendingTransactionEntry{ transaction,
                                         (uint32_t)(m_executedTransactions.size()), MinerId(), true, true });
        m_executedTransactionsSize += transaction->getSize();
        addedTransactions += 1;
    }

    // update pending blocks
    lock.unlock();
    for (auto& pendingBlock : pendingBlocks) {
        pendingBlock->addTransactions(transactions);
    }
    return addedTransactions;
}

bool PendingTransactions::addTransactionIfRequested(const Transaction_cptr& transaction, bool addToPendingBlockOnly)
{
    ASSERT(transaction);
    std::unique_lock lock(m_mutex);

    auto requestedTransactionsIterator = m_requestedTransactions.find(transaction->getId());
    if (requestedTransactionsIterator == m_requestedTransactions.end()) {
        return false;
    }

    // full copy of pending block is needed because they will be erased
    std::set<PendingBlock_ptr> pendingBlocks = requestedTransactionsIterator->second;
    m_requestedTransactions.erase(requestedTransactionsIterator);
    lock.unlock();
    for (auto& pendingBlock : pendingBlocks) {
        pendingBlock->addTransaction(transaction);
    }

    if (addToPendingBlockOnly) {
        return true;
    }

    lock.lock();
    if (m_executedTransactions.count(transaction->getId()) != 0) {
        return false;
    }

    if (m_pendingTransactions.count(transaction->getId()) != 0) {
        return false;
    }

    m_pendingTransactions.emplace(transaction->getId(),
                                  PendingTransactionEntry{
                                      .transaction = transaction
                                  });
    m_pendingTransactionsQueue.push_back(transaction);
    m_transactionsSize += transaction->getSize();
    return true;
}

Transaction_cptr PendingTransactions::getTransaction(const TransactionId& transactionId) const
{
    std::shared_lock lock(m_mutex);
    auto it = m_pendingTransactions.find(transactionId);
    if (it != m_pendingTransactions.end()) {
        return it->second.transaction;
    }
    it = m_executedTransactions.find(transactionId);
    if (it != m_executedTransactions.end()) {
        return it->second.transaction;
    }
    return nullptr;
}

std::map<TransactionId, Transaction_cptr> PendingTransactions::getTransactions(
    const std::set<TransactionId>& transactionIds) const
{
    std::shared_lock lock(m_mutex);
    std::map<TransactionId, Transaction_cptr> ret;
    for (auto& transactionId : transactionIds) {
        auto it = m_pendingTransactions.find(transactionId);
        if (it != m_pendingTransactions.end()) {
            ret.emplace(it->first, it->second.transaction);
            continue;
        }
        it = m_executedTransactions.find(transactionId);
        if (it != m_executedTransactions.end()) {
            ret.emplace(it->first, it->second.transaction);
            continue;
        }
    }
    return ret;
}


bool PendingTransactions::hasExecutedTransaction(const TransactionId& transactionId) const
{
    std::shared_lock lock(m_mutex);
    return m_executedTransactions.count(transactionId) != 0;
}

std::set<TransactionId> PendingTransactions::hasTransactions(const std::set<TransactionId>& transactionIds) const
{
    std::shared_lock lock(m_mutex);
    std::set<TransactionId> ret;
    for (auto& transactionId : transactionIds) {
        if (m_pendingTransactions.count(transactionId) != 0 || m_executedTransactions.count(transactionId) != 0) {
            ret.insert(transactionId);
        }
    }
    return ret;
}

bool PendingTransactions::isTransactionCryptoVerified(const TransactionId& transactionId) const
{
    std::shared_lock lock(m_mutex);
    auto it = m_executedTransactions.find(transactionId);
    if (it != m_executedTransactions.end()) {
        return it->second.isCryptoVerified;
    }
    auto it2 = m_pendingTransactions.find(transactionId);
    if (it2 == m_pendingTransactions.end()) {
        return false;
    }
    return it2->second.isCryptoVerified;
}

void PendingTransactions::setTransactionAsCryptoVerified(const TransactionId& transactionId)
{
    std::unique_lock lock(m_mutex);
    auto it = m_executedTransactions.find(transactionId);
    if (it != m_executedTransactions.end()) {
        it->second.isCryptoVerified = true;
        return;
    }
    auto it2 = m_pendingTransactions.find(transactionId);
    if (it2 != m_pendingTransactions.end()) {
        it2->second.isCryptoVerified = true;
    }
}

void PendingTransactions::addPendingBlock(const PendingBlock_ptr& pendingBlock)
{
    std::unique_lock lock(m_mutex);
    auto missingTransactions = pendingBlock->getMissingTransactionIds();

    // collect already existing transactions
    std::vector<Transaction_cptr> existingTransactions;
    for (auto it = missingTransactions.begin(); it != missingTransactions.end(); ) {
        Transaction_cptr transaction;
        auto transactionIterator = m_pendingTransactions.find(*it);
        if (transactionIterator != m_pendingTransactions.end()) {
            transaction = transactionIterator->second.transaction;
        } else {
            transactionIterator = m_executedTransactions.find(*it);
            if (transactionIterator == m_executedTransactions.end()) {
                ++it;
                continue;
            }
            transaction = transactionIterator->second.transaction;
        }
        it = missingTransactions.erase(it);
        existingTransactions.push_back(transaction);
    }

    // update requested transactions
    for (auto& transactionId : missingTransactions) {
        m_requestedTransactions[transactionId].insert(pendingBlock);
    }

    // unlock mutex and add existing transactions to block
    lock.unlock();
    pendingBlock->addTransactions(existingTransactions, false);
}

void PendingTransactions::removePendingBlock(const PendingBlock_ptr& pendingBlock)
{
    std::unique_lock lock(m_mutex);
    auto blockMissingTransactions = pendingBlock->getMissingTransactionIds();
    for (auto& transactionId : blockMissingTransactions) {
        auto it = m_requestedTransactions.find(transactionId);
        if (it == m_requestedTransactions.end()) {
            continue;
        }
        it->second.erase(pendingBlock);
        if (it->second.empty()) {
            m_requestedTransactions.erase(it);
        }
    }
}

std::vector<Transaction_cptr> PendingTransactions::getPendingTransactions(size_t limit) const
{
    std::shared_lock lock(m_mutex);
    if (limit > m_pendingTransactionsQueue.size()) {
        limit = m_pendingTransactionsQueue.size();
    }
    return std::vector<Transaction_cptr>(m_pendingTransactionsQueue.begin(),
                                         std::next(m_pendingTransactionsQueue.begin(), limit));
}

std::vector<Transaction_cptr> PendingTransactions::getExecutedTransactions(size_t limit) const
{
    std::shared_lock lock(m_mutex);
    if (limit > m_executedTransactions.size()) {
        limit = m_executedTransactions.size();
    }
    std::vector<Transaction_cptr> ret(limit, nullptr);
    for (auto& entry : m_executedTransactions) {
        if (entry.second.index < limit) {
            ASSERT(entry.second.transaction != nullptr);
            ASSERT(ret[entry.second.index] == nullptr);
            ret[entry.second.index] = entry.second.transaction;
        }
    }

#ifndef NDEBUG
    // make sure every transaction exists
    for (auto& transaction : ret) {
        ASSERT(transaction != nullptr);
    }
#endif

    return ret;
}

void PendingTransactions::updateTransactions(const std::vector<std::pair<TransactionId, bool>>& transactions)
{
    std::unique_lock lock(m_mutex);
    for (auto& [transactionId, isCorrect] : transactions) {
        // find_if complexity in every case should be O(1) if transaction order from getPendingTransactions is used
        auto transactionsQueueIt = std::find_if(m_pendingTransactionsQueue.begin(), m_pendingTransactionsQueue.end(),
                                                [&](const Transaction_cptr& transaction) {
            return transaction->getId() == transactionId;
        });
        ASSERT(transactionsQueueIt != m_pendingTransactionsQueue.end());
        Transaction_cptr transaction = *transactionsQueueIt;
        ASSERT(transaction);
        m_pendingTransactionsQueue.erase(transactionsQueueIt);

        if (isCorrect) {
            // transaction has been executed correctly, move to executed transactions
            auto it = m_pendingTransactions.find(transaction->getId());
            ASSERT(it != m_pendingTransactions.end());
            ASSERT(m_executedTransactions.count(it->first) == 0);
            m_executedTransactions.emplace(it->first, PendingTransactionEntry{ transaction,
                                           (uint32_t)(m_executedTransactions.size()), MinerId(), true, true });
            m_executedTransactionsSize += it->second.transaction->getSize();
            m_pendingTransactions.erase(it);
        } else {
            // cannot execute transaction, remove from pending transactions
            auto it = m_pendingTransactions.find(transaction->getId());
            ASSERT(it != m_pendingTransactions.end());
            m_transactionsSize -= transaction->getSize();
            m_pendingTransactions.erase(it);
        }
    }
}

void PendingTransactions::removeTransactions(const std::set<TransactionId>& transactionIds)
{
    std::unique_lock lock(m_mutex);
    for (auto& transactionId : transactionIds) {
        auto it = m_pendingTransactions.find(transactionId);
        if (it == m_pendingTransactions.end()) {
            continue;
        }
        m_pendingTransactions.erase(it);
        m_transactionsSize -= transactionId.getSize();
    }

    auto it = std::remove_if(m_pendingTransactionsQueue.begin(), m_pendingTransactionsQueue.end(),
                             [&](const Transaction_cptr& transaction) {
        if (transactionIds.count(transaction->getId()) != 0) {
            return true;
        }
        return false;
    });
    m_pendingTransactionsQueue.erase(it, m_pendingTransactionsQueue.end());
}

void PendingTransactions::clearExecutedTransactions()
{
    std::unique_lock lock(m_mutex);
    std::vector<Transaction_cptr> transactions(m_executedTransactions.size(), nullptr);
    for (auto& entry : m_executedTransactions) {
        transactions[entry.second.index] = entry.second.transaction;
    }

    for (auto& transaction : transactions) {
        ASSERT(transaction != nullptr);
        ASSERT(m_pendingTransactions.count(transaction->getId()) == 0);
        m_pendingTransactions.emplace(transaction->getId(),
                                      PendingTransactionEntry{ transaction, 0, MinerId(), true, true });
    }

    m_pendingTransactionsQueue.insert(m_pendingTransactionsQueue.begin(), transactions.begin(), transactions.end());
    m_executedTransactions.clear();
    m_executedTransactionsSize = 0;
}

uint32_t PendingTransactions::getTransactionsCount() const
{
    std::shared_lock lock(m_mutex);
    return m_pendingTransactions.size() + m_executedTransactions.size();
}

uint32_t PendingTransactions::getTransactionsSize() const
{
    std::shared_lock lock(m_mutex);
    return m_transactionsSize;
}

// returns count of executed pending transactions
uint32_t PendingTransactions::getPendingTransactionsCount() const
{
    std::shared_lock lock(m_mutex);
    return m_pendingTransactions.size();
}

// returns size of executed pending transactions
uint32_t PendingTransactions::getPendingTransactionsSize() const
{
    std::shared_lock lock(m_mutex);
    return m_transactionsSize - m_executedTransactionsSize;
}

uint32_t PendingTransactions::getExecutedTransactionsCount() const
{
    std::shared_lock lock(m_mutex);
    return m_executedTransactions.size();
}

uint32_t PendingTransactions::getExecutedTransactionsSize() const
{
    std::shared_lock lock(m_mutex);
    return m_executedTransactionsSize;
}

uint32_t PendingTransactions::getRequestedTransactionsCount() const
{
    std::shared_lock lock(m_mutex);
    return m_requestedTransactions.size();
}

uint32_t PendingTransactions::getMaxPendingTransactionsCount() const
{
    return kBlockMaxTransactions * 2;
}

uint32_t PendingTransactions::getMaxPendingTransactionsSize() const
{
    return kBlockMaxTransactionsSize * 8;
}

json PendingTransactions::getDebugInfo() const
{
    std::shared_lock lock(m_mutex);
    return {
        {"executed_transactions", m_executedTransactions.size()},
        {"executed_transactions_size", m_executedTransactionsSize},
        {"pending_transactions", m_pendingTransactions.size()},
        {"pending_transactions_size", getPendingTransactionsSize()},
        {"requested_transactions", m_requestedTransactions.size()},
    };
}

}
