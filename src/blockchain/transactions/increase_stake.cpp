#include "pch.h"
#include "increase_stake.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr IncreaseStakeTransaction::create(uint32_t blockId, int16_t pricing, const MinerId& minerId,
                                                 uint64_t value)
{
    auto transaction = std::make_shared<IncreaseStakeTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_minerId = minerId;
    transaction->m_value = value;
    transaction->reload();
    return transaction;
}

void IncreaseStakeTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    if (m_value == 0) {
        THROW_TRANSACTION_EXCEPTION("Invalid value");
    }

    if (!database.miners.getMiner(m_minerId)) {
        THROW_TRANSACTION_EXCEPTION("Provided miner does not exists");
    }
}

void IncreaseStakeTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    Miner_ptr miner = database.miners.getMiner(m_minerId)->clone(blockId);
    miner->addStake(m_value, false);

    database.miners.updateMiner(miner);

    Transaction::execute(blockId, database);
}

uint64_t IncreaseStakeTransaction::getCost() const noexcept
{
    return m_value;
}

void IncreaseStakeTransaction::serializeBody(Serializer& s)
{
    s(m_minerId);
    s(m_value);
}

void IncreaseStakeTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["miner_id"] = m_minerId;
    j["value"] = m_value;
}

}
