#include "pch.h"
#include "transfer.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr TransferTransaction::create(uint32_t blockId, int16_t pricing, const UserId& destinationUser,
                                            uint64_t value)
{
    auto transaction = std::make_shared<TransferTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_destination = destinationUser;
    transaction->m_value = value;
    transaction->reload();
    return transaction;
}

void TransferTransaction::preload(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    database.users.preloadUser(m_destination);
    Transaction::preload(blockId, database);
}

void TransferTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    if (m_value == 0) {
        THROW_TRANSACTION_EXCEPTION("Transfer transaction has invalid transfer amount");
    }
    if (m_destination == getUserId()) {
        THROW_TRANSACTION_EXCEPTION("Transfer transaction user can not transfer tokens to himself");
    }
    if (m_signatures.getType() == MultiSignaturesTypes::SPONSOR && m_signatures.getSponsorId() == m_destination) {
        THROW_TRANSACTION_EXCEPTION("Transfer transaction sponsor can not sponsor transfering tokens to himself");
    }
    if (!database.users.getUser(m_destination)) {
        THROW_TRANSACTION_EXCEPTION("Transfer transaction destination user does not exist");
    }
}

void TransferTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = database.users.getUser(m_destination)->clone(blockId);
    user->tokens += m_value;
    database.users.addUserHistory(user->getId(), user->operations / 100,
                                  UserHistory(blockId, UserHistoryType::INCOMING_TRANSACTION, getId()));
    user->operations += 1;
    database.users.updateUser(user);

    Transaction::execute(blockId, database);
}

uint64_t TransferTransaction::getCost() const noexcept
{
    return m_value;
}

void TransferTransaction::serializeBody(Serializer& s)
{
    s(m_destination);
    s(m_value);
}

void TransferTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["destination"] = m_destination;
    j["value"] = m_value;
}

}
