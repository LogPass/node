#include "pch.h"
#include "unlock_user.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr UnlockUserTransaction::create(uint32_t blockId, int16_t pricing,
                                              const std::set<PublicKey>& keysToUnlock,
                                              const std::set<UserId>& supervisorsToUnlock)
{
    auto transaction = std::make_shared<UnlockUserTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_keysToUnlock = keysToUnlock;
    transaction->m_supervisorsToUnlock = supervisorsToUnlock;
    transaction->reload();
    return transaction;
}

void UnlockUserTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    User_cptr user = database.users.getUser(getUserId());

    if (m_keysToUnlock.empty() && m_supervisorsToUnlock.empty()) {
        THROW_TRANSACTION_EXCEPTION("No key or supervisor to unlock was provided");
    }

    bool hasValidUnlock = false;
    for (auto& key : m_keysToUnlock) {
        if (!user->hasKey(key)) {
            THROW_TRANSACTION_EXCEPTION("Provided key is not a part of user account");
        }
        if (user->lockedKeys.contains(key)) {
            hasValidUnlock = true;
        }
    }

    for (auto& supervisorId : m_supervisorsToUnlock) {
        if (!user->hasSupervisor(supervisorId)) {
            THROW_TRANSACTION_EXCEPTION("Provided supervisor is not a part of user account");
        }
        if (user->lockedSupervisors.contains(supervisorId)) {
            hasValidUnlock = true;
        }
    }

    if (!hasValidUnlock) {
        THROW_TRANSACTION_EXCEPTION("Provided keys and supervisors to unlock are already unlocked");
    }
}

void UnlockUserTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = database.users.getUser(getUserId())->clone(blockId);
    for (auto& key : m_keysToUnlock) {
        user->lockedKeys.erase(key);
    }
    for (auto& supervisorId : m_supervisorsToUnlock) {
        user->lockedSupervisors.erase(supervisorId);
    }
    database.users.updateUser(user);

    Transaction::execute(blockId, database);
}

void UnlockUserTransaction::serializeBody(Serializer& s)
{
    s.serialize<uint8_t>(m_keysToUnlock);
    s.serialize<uint8_t>(m_supervisorsToUnlock);
}

void UnlockUserTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["keys_to_unlock"] = m_keysToUnlock;
    j["supervisors_to_unlock"] = m_supervisorsToUnlock;
}

}
