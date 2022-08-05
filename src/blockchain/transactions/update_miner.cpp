#include "pch.h"
#include "update_miner.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr UpdateMinerTransaction::create(uint32_t blockId, int16_t pricing, const MinerId& miner,
                                               const MinerSettings& settings)
{
    auto transaction = std::make_shared<UpdateMinerTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_minerId = miner;
    transaction->m_settings = settings;
    transaction->reload();
    return transaction;
}

void UpdateMinerTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);
    if (!m_minerId.isValid()) {
        THROW_TRANSACTION_EXCEPTION("Invalid miner");
    }
    Miner_cptr miner = database.miners.getMiner(m_minerId);
    if (!miner) {
        THROW_TRANSACTION_EXCEPTION("Invalid miner");
    }
    if (miner->owner != getUserId()) {
        THROW_TRANSACTION_EXCEPTION("Requested miner doesn't belong to this user");
    }
}

void UpdateMinerTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    Miner_ptr miner = database.miners.getMiner(m_minerId)->clone(blockId);
    miner->settings = m_settings;
    database.miners.updateMiner(miner);

    Transaction::execute(blockId, database);
}

void UpdateMinerTransaction::serializeBody(Serializer& s)
{
    s(m_minerId);
    s(m_settings);
}

void UpdateMinerTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["miner_id"] = m_minerId;
    j["settings"] = m_settings;
}

}
