#include "pch.h"

#include "pending_block.h"

namespace logpass {

PendingBlock::Status PendingBlock::getStatus() const
{
    std::shared_lock lock(m_mutex);
    if (m_invalid) {
        return Status::INVALID;
    }
    if (m_finished) {
        return Status::FINISHED;
    }
    if (m_expired) {
        return Status::EXPIRED;
    }
    if (!m_body) {
        return Status::MISSING_BODY;
    }
    ASSERT(m_body->getHashes().size() == m_transactionIds.size());
    if (std::find(m_transactionIds.begin(), m_transactionIds.end(), nullptr) != m_transactionIds.end()) {
        return Status::MISSING_TRANSACTION_IDS;
    }
    if (!m_missingTransactions.empty()) {
        return Status::MISSING_TRANSACTIONS;
    }
    return Status::COMPLETE;
}

Hash PendingBlock::getBlockBodyHash() const
{
    return m_header->getBodyHash();
}

PendingBlock::AddResult PendingBlock::addBlockBody(const BlockBody_cptr& blockBody)
{
    std::unique_lock lock(m_mutex);
    if (m_invalid) {
        return PendingBlock::AddResult::INVALID_BLOCK;
    }
    if (blockBody->getHash() != m_header->getBodyHash()) {
        return PendingBlock::AddResult::INVALID_DATA;
    }
    if (m_body) {
        return PendingBlock::AddResult::DUPLICATED;
    }

    m_body = blockBody;
    m_transactionIds.resize(m_body->getHashes().size());

    lock.unlock();
    m_onUpdated(shared_from_this());

    return PendingBlock::AddResult::CORRECT;
}

std::vector<std::pair<uint32_t, Hash>> PendingBlock::getMissingTransactionIdsHashes(uint32_t limit) const
{
    std::shared_lock lock(m_mutex);
    if (!m_body) {
        return {};
    }

    const std::vector<Hash>& hashes = m_body->getHashes();
    ASSERT(hashes.size() == m_transactionIds.size());

    std::vector<std::pair<uint32_t, Hash>> ret;
    for (uint32_t i = 0; i < hashes.size(); ++i) {
        if (!m_transactionIds[i]) {
            ret.push_back(std::make_pair(i, hashes[i]));
            if (ret.size() == limit) {
                break;
            }
        }
    }
    return ret;
}

PendingBlock::AddResult PendingBlock::addBlockTransactionIds(
    const std::vector<BlockTransactionIds_cptr>& blockTransactionIds)
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_body);
    if (m_invalid) {
        return PendingBlock::AddResult::INVALID_BLOCK;
    }

    const std::vector<Hash>& hashes = m_body->getHashes();
    ASSERT(hashes.size() == m_transactionIds.size());

    bool newData = false;
    for (auto& transactionIds : blockTransactionIds) {
        uint32_t index = hashes.size();
        for (uint32_t i = 0; i < hashes.size(); ++i) {
            if (hashes[i] == transactionIds->getHash()) {
                index = i;
                break;
            }
        }

        if (index == hashes.size()) {
            return PendingBlock::AddResult::INVALID_DATA;
        }

        if (m_transactionIds[index]) {
            continue;
        }

        m_transactionIds[index] = transactionIds;
        newData = true;
    }

    if (!newData) {
        return PendingBlock::AddResult::DUPLICATED;
    }

    if (std::find(m_transactionIds.begin(), m_transactionIds.end(), nullptr) == m_transactionIds.end()) {
        // we have all transaction ids, can create missing transactions now
        ASSERT(m_missingTransactions.empty());
        for (auto& blockTransactionIds : m_transactionIds) {
            for (auto& transactionId : *blockTransactionIds) {
                m_missingTransactions.insert(transactionId);
            }
        }
    }

    lock.unlock();
    m_onUpdated(shared_from_this());

    return PendingBlock::AddResult::CORRECT;
}

std::set<TransactionId> PendingBlock::getTransactionIds() const
{
    std::shared_lock lock(m_mutex);
    if (!m_body) {
        return {};
    }

    if (std::find(m_transactionIds.begin(), m_transactionIds.end(), nullptr) != m_transactionIds.end()) {
        return {};
    }

    std::set<TransactionId> ret;

    for (auto& transactionIds : m_transactionIds) {
        ret.insert(transactionIds->begin(), transactionIds->end());
    }

    return ret;
}

std::set<TransactionId> PendingBlock::getMissingTransactionIds(uint32_t countLimit, uint32_t sizeLimit) const
{
    std::shared_lock lock(m_mutex);
    std::set<TransactionId> ret;
    uint32_t size = 0;
    for (auto& transactionId : m_missingTransactions) {
        if (ret.size() >= countLimit && countLimit != 0) {
            break;
        }
        if (transactionId.getSize() + size > sizeLimit && sizeLimit != 0) {
            continue;
        }
        ret.insert(transactionId);
        size += transactionId.getSize();
    }
    return ret;
}

PendingBlock::AddResult PendingBlock::addTransactions(const std::vector<Transaction_cptr>& transactions,
                                                      bool executeCallback)
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_body && std::find(m_transactionIds.begin(), m_transactionIds.end(), nullptr) == m_transactionIds.end());
    if (m_invalid) {
        return PendingBlock::AddResult::INVALID_BLOCK;
    }

    if (transactions.empty()) {
        return PendingBlock::AddResult::INVALID_DATA;
    }

    size_t newTransactions = 0;
    for (auto& transaction : transactions) {
        auto it = m_missingTransactions.find(transaction->getId());
        if (it == m_missingTransactions.end()) {
            continue;
        }
        m_missingTransactions.erase(it);
        m_transactions.emplace(transaction->getId(), transaction);
        newTransactions += 1;
    }

    if (newTransactions == 0) {
        // no new transactions, check if input is duplicated or invalid
        for (auto& transaction : transactions) {
            if (m_transactions.count(transaction->getId()) != 0) {
                return PendingBlock::AddResult::DUPLICATED;
            }
        }
        return PendingBlock::AddResult::INVALID_DATA;
    }

    lock.unlock();
    if (executeCallback) {
        m_onUpdated(shared_from_this());
    }

    return PendingBlock::AddResult::CORRECT;
}

bool PendingBlock::hasTransaction(const TransactionId& transactionId) const
{
    std::shared_lock lock(m_mutex);
    return m_transactions.count(transactionId) != 0;
}

void PendingBlock::setInvalid()
{
    std::unique_lock lock(m_mutex);
    if (m_invalid) {
        return;
    }

    m_invalid = true;
}

bool PendingBlock::isInvalid()
{
    std::shared_lock lock(m_mutex);
    return m_invalid;
}

void PendingBlock::setExpired()
{
    std::unique_lock lock(m_mutex);
    if (m_invalid) {
        return;
    }

    m_expired = true;
}

bool PendingBlock::isExpired()
{
    std::shared_lock lock(m_mutex);
    return m_expired;
}

void PendingBlock::setFinished()
{
    std::unique_lock lock(m_mutex);
    if (m_invalid) {
        return;
    }

    m_finished = true;
}

bool PendingBlock::isFinished()
{
    std::shared_lock lock(m_mutex);
    return m_finished;
}

Block_cptr PendingBlock::createBlock() const
{
    std::shared_lock lock(m_mutex);
    if (m_invalid) {
        return nullptr;
    } else if (m_expired) {
        return nullptr;
    } else if (!m_body) {
        return nullptr;
    } else if (std::find(m_transactionIds.begin(), m_transactionIds.end(), nullptr) != m_transactionIds.end()) {
        return nullptr;
    } else if (!m_missingTransactions.empty()) {
        return nullptr;
    } else if (m_transactions.size() != m_body->getTransactions()) {
        return nullptr;
    }

    return std::make_shared<Block>(m_header, m_body, m_transactionIds, m_transactions);
}

}
