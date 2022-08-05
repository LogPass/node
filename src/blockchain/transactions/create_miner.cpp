#include "pch.h"
#include "create_miner.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr CreateMinerTransaction::create(uint32_t blockId, int16_t pricing, const MinerId& minerId)
{
    auto transaction = std::make_shared<CreateMinerTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_minerId = minerId;
    transaction->reload();
    return transaction;
}

void CreateMinerTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    if (!m_minerId.isValid()) {
        THROW_TRANSACTION_EXCEPTION("Invalid miner id");
    }

    if (database.miners.getMiner(m_minerId)) {
        THROW_TRANSACTION_EXCEPTION("Miner already exist");
    }
}

void CreateMinerTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    Miner_ptr miner = Miner::create(m_minerId, getUserId(), blockId);
    database.miners.addMiner(miner);

    Transaction::execute(blockId, database);
}

void CreateMinerTransaction::serializeBody(Serializer& s)
{
    s(m_minerId);
}

void CreateMinerTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["miner_id"] = m_minerId;
}

}
