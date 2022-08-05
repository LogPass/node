#include "pch.h"
#include "select_miner.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr SelectMinerTransaction::create(uint32_t blockId, int16_t pricing, const MinerId& miner)
{
    auto transaction = std::make_shared<SelectMinerTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_minerId = miner;
    transaction->reload();
    return transaction;
}

void SelectMinerTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);
    if (!m_minerId.isValid() || !database.miners.getMiner(m_minerId)) {
        THROW_TRANSACTION_EXCEPTION("Invalid miner");
    }

    User_cptr user = database.users.getUser(getUserId());
    if (!user || user->miner == m_minerId) {
        THROW_TRANSACTION_EXCEPTION("Requested miner is already set");
    }
}

void SelectMinerTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = database.users.getUser(getUserId())->clone(blockId);
    user->miner = m_minerId;
    database.users.updateUser(user);

    Transaction::execute(blockId, database);
}

void SelectMinerTransaction::serializeBody(Serializer& s)
{
    s(m_minerId);
}

void SelectMinerTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["miner_id"] = m_minerId;
}

}
