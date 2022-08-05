#include "pch.h"
#include "lock_user.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr LockUserTransaction::create(uint32_t blockId, int16_t pricing, const std::set<PublicKey>& keysToLock,
                                            const std::set<UserId>& supervisorsToLock)
{
    auto transaction = std::make_shared<LockUserTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_keysToLock = keysToLock;
    transaction->m_supervisorsToLock = supervisorsToLock;
    transaction->reload();
    return transaction;
}

void LockUserTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    User_cptr user = database.users.getUser(getUserId());

    if (m_keysToLock.empty() && m_supervisorsToLock.empty()) {
        THROW_TRANSACTION_EXCEPTION("No key or supervisor to lock was provided");
    }

    bool hasValidLock = false;
    for (auto& key : m_keysToLock) {
        if (!user->hasKey(key)) {
            THROW_TRANSACTION_EXCEPTION("Provided key is not a part of user account");
        }
        if (!user->lockedKeys.contains(key)) {
            hasValidLock = true;
        }
    }

    for (auto& supervisorId : m_supervisorsToLock) {
        if (!user->hasSupervisor(supervisorId)) {
            THROW_TRANSACTION_EXCEPTION("Provided supervisor is not a part of user account");
        }
        if (!user->lockedSupervisors.contains(supervisorId)) {
            hasValidLock = true;
        }
    }

    if (!hasValidLock) {
        THROW_TRANSACTION_EXCEPTION("Provided keys and supervisors to unlock are already locked");
    }
}

void LockUserTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = database.users.getUser(getUserId())->clone(blockId);
    for (auto& key : m_keysToLock) {
        user->lockedKeys.insert(key);
    }
    for (auto& supervisorId : m_supervisorsToLock) {
        user->lockedSupervisors.insert(supervisorId);
    }
    database.users.updateUser(user);

    Transaction::execute(blockId, database);
}

void LockUserTransaction::serializeBody(Serializer& s)
{
    s.serialize<uint8_t>(m_keysToLock);
    s.serialize<uint8_t>(m_supervisorsToLock);
}

void LockUserTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["keys_to_lock"] = m_keysToLock;
    j["supervisors_to_lock"] = m_supervisorsToLock;
}

}
