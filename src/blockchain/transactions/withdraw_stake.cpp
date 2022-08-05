#include "pch.h"
#include "withdraw_stake.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr WithdrawStakeTransaction::create(uint32_t blockId, int16_t pricing, const MinerId& minerId,
                                                 uint64_t unlockedStake, uint64_t lockedStake)
{
    auto transaction = std::make_shared<WithdrawStakeTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_minerId = minerId;
    transaction->m_unlockedStake = unlockedStake;
    transaction->m_lockedStake = lockedStake;
    transaction->reload();
    return transaction;
}

void WithdrawStakeTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    auto miner = database.miners.getMiner(m_minerId);
    if (!miner) {
        THROW_TRANSACTION_EXCEPTION("Provided miner does not exists");
    }

    if (miner->owner != getUserId()) {
        THROW_TRANSACTION_EXCEPTION("Provided miner is not owned by transaction user");
    }

    if (m_lockedStake == 0 && m_unlockedStake == 0) {
        THROW_TRANSACTION_EXCEPTION("Stake to widthdraw from miner is invalid (0)");
    }

    if (m_lockedStake > miner->lockedStake) {
        THROW_TRANSACTION_EXCEPTION("Miner does not have enough locked stake to withdraw from it");
    }

    if (m_unlockedStake > (miner->stake - miner->lockedStake)) {
        THROW_TRANSACTION_EXCEPTION("Miner does not have enough unlocked stake to withdraw from it");
    }
}

void WithdrawStakeTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    Miner_ptr miner = database.miners.getMiner(m_minerId)->clone(blockId);
    miner->withdrawStake(m_unlockedStake, m_lockedStake);
    database.miners.updateMiner(miner);

    User_ptr user = database.users.getUser(getUserId())->clone(blockId);
    user->tokens += m_unlockedStake + (m_lockedStake * 19) / 20;
    database.users.updateUser(user);

    Transaction::execute(blockId, database);
}

void WithdrawStakeTransaction::serializeBody(Serializer& s)
{
    s(m_minerId);
    s(m_unlockedStake);
    s(m_lockedStake);
}

void WithdrawStakeTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["miner_id"] = m_minerId;
    j["unlocked_stake"] = m_unlockedStake;
    j["locked_stake"] = m_lockedStake;
}

}
