#include "pch.h"
#include "commit.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr CommitTransaction::create(uint32_t blockId, int16_t pricing, const MinerId& minerId,
                                          uint32_t transactions, uint64_t users, uint64_t tokens, uint64_t stakedTokens)
{
    auto transaction = std::make_shared<CommitTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_minerId = minerId;
    transaction->m_transactions = transactions;
    transaction->m_users = users;
    transaction->m_tokens = tokens;
    transaction->m_stakedTokens = stakedTokens;
    transaction->m_reward = getMiningReward(pricing, transactions, users, tokens, stakedTokens);
    transaction->reload();
    return transaction;
}

void CommitTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    if (blockId != m_blockId) {
        THROW_TRANSACTION_EXCEPTION("Invalid block id");
    }

    uint32_t miningQueueIndex = blockId - database.blocks.getLatestBlockId() - 1;
    auto miningQueue = database.blocks.getMinersQueue();

    if (miningQueueIndex >= miningQueue.size()) {
        THROW_TRANSACTION_EXCEPTION("Invalid index in mining queue");
    }

    if (m_minerId != miningQueue[miningQueueIndex]) {
        THROW_TRANSACTION_EXCEPTION("Invalid miner id");
    }

    if (m_signatures.getSize() != 1) {
        THROW_TRANSACTION_EXCEPTION("Invalid signatures for commit transaction");
    }

    if (database.transactions.getNewTransactionsCount() != m_transactions) {
        THROW_TRANSACTION_EXCEPTION("Invalid number of new transactions");
    }

    if (database.users.getUsersCount() != m_users) {
        THROW_TRANSACTION_EXCEPTION("Invalid number of existing users");
    }

    if (database.users.getTokens() != m_tokens) {
        THROW_TRANSACTION_EXCEPTION("Invalid number existing tokens");
    }

    if (database.miners.getStakedTokens() != m_stakedTokens) {
        THROW_TRANSACTION_EXCEPTION("Invalid number of staked tokens");
    }

    uint64_t reward = getMiningReward(m_pricing, m_transactions, m_users, m_tokens, m_stakedTokens);
    if (reward != m_reward) {
        THROW_TRANSACTION_EXCEPTION("Invalid value of reward");
    }

    if (database.transactions.getNewTransactionsCountByType(getType()) > 0) {
        THROW_TRANSACTION_EXCEPTION("Miner already received reward for this block");
    }
}

void CommitTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    Miner_ptr miner = database.miners.getMiner(m_minerId)->clone(blockId);
    miner->stake += m_reward;
    miner->unlockStake(blockId);
    database.miners.updateMiner(miner);

    Transaction::execute(blockId, database);
}

void CommitTransaction::serializeBody(Serializer& s)
{
    s(m_version);
    if (m_version != 1) {
        THROW_SERIALIZER_EXCEPTION("Invalid version");
    }
    s(m_minerId);
    s(m_transactions);
    s(m_users);
    s(m_tokens);
    s(m_stakedTokens);
    s(m_reward);
}

void CommitTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["miner_id"] = m_minerId;
    j["transactions"] = m_transactions;
    j["users"] = m_users;
    j["tokens"] = m_tokens;
    j["staked_tokens"] = m_stakedTokens;
    j["reward"] = m_reward;
}

uint64_t CommitTransaction::getMiningReward(int16_t pricing, uint32_t newTransactions, uint64_t users, uint64_t tokens,
                                            uint64_t stakedTokens)
{
    ASSERT(pricing > 0);
    uint64_t maxTokens = kFirstUserBalance + kFirstUserStake;
    uint64_t missingTokens = maxTokens - tokens - stakedTokens;
    uint64_t transactionFee = (kTransactionFee * 25) / (24 + pricing);
    uint64_t transactionsReward = newTransactions * transactionFee / 5;
    uint64_t missingTokensReward = missingTokens / (kBlocksPerDay * kStakingDuration / 2);
    uint64_t reward = std::min<uint64_t>(missingTokens, transactionsReward + missingTokensReward);
    return reward;
}

}
