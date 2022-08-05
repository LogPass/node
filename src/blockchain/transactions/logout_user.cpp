#include "pch.h"
#include "logout_user.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr LogoutUserTransaction::create(uint32_t blockId, int16_t pricing)
{
    auto transaction = std::make_shared<LogoutUserTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->reload();
    return transaction;
}

void LogoutUserTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);
}

void LogoutUserTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = database.users.getUser(getUserId())->clone(blockId);
    user->logout = blockId;
    database.users.updateUser(user);

    Transaction::execute(blockId, database);
}

void LogoutUserTransaction::serializeBody(Serializer& s)
{

}

}
