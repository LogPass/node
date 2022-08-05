#include "pch.h"
#include "update_user.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr UpdateUserTransaction::create(uint32_t blockId, int16_t pricing, const UserSettings& settings)
{
    auto transaction = std::make_shared<UpdateUserTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_settings = settings;
    transaction->reload();
    return transaction;
}

void UpdateUserTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    User_cptr user = database.users.getUser(getUserId());
    if (!user) {
        THROW_TRANSACTION_EXCEPTION("Invalid user");
    }

    std::set<User_cptr> supervisors;
    for (auto& [supervisorId, supervisorSettings] : user->settings.supervisors) {
        supervisors.insert(database.users.getUser(supervisorId));
    }

    std::set<PublicKey> usedKeys;
    PowerLevel powerLevel = user->getPowerLevel(m_signatures, supervisors, usedKeys,
                                                getTransactionSettings().ignoresLock);
    try {
        user->validateNewSettings(powerLevel, m_settings);
    } catch (const UserSettingsValidationError& error) {
        THROW_TRANSACTION_EXCEPTION(error.what());
    }
}

void UpdateUserTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = database.users.getUser(getUserId())->clone(blockId);

    std::set<User_cptr> supervisors;
    for (auto& [supervisorId, supervisorSettings] : user->settings.supervisors) {
        supervisors.insert(database.users.getUser(supervisorId));
    }

    std::set<PublicKey> usedKeys;
    PowerLevel powerLevel = user->getPowerLevel(m_signatures, supervisors, usedKeys,
                                                getTransactionSettings().ignoresLock);

    user->setPendingUpdate(powerLevel, m_settings, blockId, getId());
    database.users.updateUser(user);
    Transaction::execute(blockId, database);
}

void UpdateUserTransaction::serializeBody(Serializer& s)
{
    s(m_settings);
}

void UpdateUserTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["settings"] = m_settings;
}

}
