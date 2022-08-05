#include "pch.h"
#include "init.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr InitTransaction::create(uint32_t blockId, int16_t pricing, uint64_t initializationTime,
                                        uint32_t blockInterval)
{
    auto transaction = std::make_shared<InitTransaction>();
    transaction->m_version = 1;
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_initializationTime = initializationTime;
    transaction->m_blockInterval = blockInterval;
    transaction->reload();
    return transaction;
}

void InitTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    if (m_blockId != 1 || blockId != 1) {
        THROW_TRANSACTION_EXCEPTION("Init transaction can be executed only in first block");
    }

    if (m_pricing != 0) {
        THROW_TRANSACTION_EXCEPTION("Init transaction must have pricing set to 0");
    }

    if (m_version != 1) {
        THROW_TRANSACTION_EXCEPTION("Invalid version");
    }

    if (database.blocks.getLatestBlockId() != 0) {
        THROW_TRANSACTION_EXCEPTION("Blockchain is already initialized");
    }

    if (database.transactions.getTransactionsCount() != 0) {
        THROW_TRANSACTION_EXCEPTION("Blockchain is already initialized");
    }

    if (database.users.getUsersCount() != 0) {
        THROW_TRANSACTION_EXCEPTION("Blockchain is already initialized");
    }

    if (m_initializationTime == 0) {
        THROW_TRANSACTION_EXCEPTION("Invalid initialization time");
    }

    if (m_initializationTime % 60 != 0) {
        THROW_TRANSACTION_EXCEPTION("Initialization time can't contain seconds");
    }

    if (m_blockInterval != kBlockInterval) {
        THROW_TRANSACTION_EXCEPTION("Invalid block interval");
    }

    if (m_signatures.getSize() != 1 || UserId(m_signatures.getPublicKey()) != getUserId()) {
        THROW_TRANSACTION_EXCEPTION("Invalid signatures for init transaction");
    }

    if (!validateSignatures()) {
        THROW_TRANSACTION_EXCEPTION("Validation of signatures failed");
    }

#ifndef NDEBUG
    m_validated = blockId;
#endif
}

void InitTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = User::create(m_signatures.getPublicKey(), UserId(m_signatures.getPublicKey()), blockId,
                                 kFirstUserBalance);
    user->miner = MinerId(m_signatures.getPublicKey());
    database.users.addUser(user);

    Miner_ptr miner = Miner::create(MinerId(m_signatures.getPublicKey()), user->getId(), blockId);
    miner->stake = kFirstUserStake;
    database.miners.addMiner(miner);

    Transaction::execute(blockId, database);
}

void InitTransaction::serializeBody(Serializer& s)
{
    s(m_version);
    s(m_initializationTime);
    s(m_blockInterval);
}

void InitTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["version"] = m_version;
    j["initialization_time"] = m_initializationTime;
    j["block_interval"] = m_blockInterval;
}

}
