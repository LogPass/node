#include "pch.h"
#include "create_user.h"
#include <database/database.h>
#include <models/user/user_sponsor.h>

namespace logpass {

Transaction_ptr CreateUserTransaction::create(uint32_t blockId, int16_t pricing, const PublicKey& publicKey,
                                              uint8_t sponsoredTransactions, const Hash& sponsor)
{
    auto transaction = std::make_shared<CreateUserTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_publicKey = publicKey;
    transaction->m_sponsoredTransactions = sponsoredTransactions;
    transaction->m_sponsor = sponsor;
    transaction->reload();
    return transaction;
}

void CreateUserTransaction::preload(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    database.users.preloadUser(UserId(m_publicKey));
    Transaction::preload(blockId, database);
}

void CreateUserTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);
    if (!m_publicKey.isValid()) {
        THROW_TRANSACTION_EXCEPTION("Invalid user id");
    }

    if (database.users.getUser(UserId(m_publicKey))) {
        THROW_TRANSACTION_EXCEPTION("User already exist");
    }

    if (m_sponsoredTransactions < kUserMinFreeTransactions || m_sponsoredTransactions > kUserMaxFreeTransactions) {
        THROW_TRANSACTION_EXCEPTION("Invalid number of sponsored transactions");
    }
}

void CreateUserTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    User_ptr user = User::create(m_publicKey, getUserId(), blockId, 0, m_sponsoredTransactions);
    database.users.addUserHistory(user->getId(), user->operations / 100,
                                  UserHistory(blockId, UserHistoryType::INCOMING_TRANSACTION, getId()));
    user->operations += 1;
    database.users.addUserSponsor(user->getId(), user->sponsors / 100,
                                  UserSponsor(blockId, m_sponsor, m_sponsoredTransactions));
    user->sponsors += 1;
    database.users.addUser(user);

    Transaction::execute(blockId, database);
}

uint64_t CreateUserTransaction::getFee() const noexcept
{
    return Transaction::getFee() * ((uint64_t)m_sponsoredTransactions + 1);
}

void CreateUserTransaction::serializeBody(Serializer& s)
{
    s(m_publicKey);
    s(m_sponsoredTransactions);
    s(m_sponsor);
}

void CreateUserTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["public_key"] = m_publicKey;
    j["new_user_id"] = UserId(m_publicKey);
    j["sponsored_transactions"] = m_sponsoredTransactions;
    j["sponsor"] = m_sponsor;
}

}
