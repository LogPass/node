#include "pch.h"
#include "sponsor_user.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr SponsorUserTransaction::create(uint32_t blockId, int16_t pricing, const UserId& userId,
                                             uint8_t sponsoredTransactions, const Hash& sponsor)
{
    auto transaction = std::make_shared<SponsorUserTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_userId = userId;
    transaction->m_sponsoredTransactions = sponsoredTransactions;
    transaction->m_sponsor = sponsor;
    transaction->reload();
    return transaction;
}

void SponsorUserTransaction::preload(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    database.users.preloadUser(m_userId);
    Transaction::preload(blockId, database);
}

void SponsorUserTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);

    if (!database.users.getUser(m_userId)) {
        THROW_TRANSACTION_EXCEPTION("User doest not exist");
    }

    if (m_sponsoredTransactions == 0 || m_sponsoredTransactions > kUserMaxFreeTransactions) {
        THROW_TRANSACTION_EXCEPTION("Invalid number of sponsored transactions");
    }

    if (m_signatures.getType() == MultiSignaturesTypes::SPONSOR && m_signatures.getSponsorId() == m_userId) {
        THROW_TRANSACTION_EXCEPTION("Sponsor user transaction sponsor can not sponsor himself");
    }
}

void SponsorUserTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    auto user = database.users.getUser(m_userId)->clone(blockId);
    user->freeTransactions = std::min<uint8_t>(user->freeTransactions + m_sponsoredTransactions,
                                               kUserMaxFreeTransactions);
    database.users.addUserHistory(user->getId(), user->operations / 100,
                                  UserHistory(blockId, UserHistoryType::INCOMING_TRANSACTION, getId()));
    user->operations += 1;
    database.users.addUserSponsor(user->getId(), user->sponsors / 100,
                                  UserSponsor(blockId, m_sponsor, m_sponsoredTransactions));
    user->sponsors += 1;
    database.users.updateUser(user);

    Transaction::execute(blockId, database);
}

uint64_t SponsorUserTransaction::getFee() const noexcept
{
    return Transaction::getFee() * ((uint64_t)m_sponsoredTransactions + 1);
}

void SponsorUserTransaction::serializeBody(Serializer& s)
{
    s(m_userId);
    s(m_sponsoredTransactions);
    s(m_sponsor);
}

void SponsorUserTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["renewed_user_id"] = m_userId;
    j["sponsored_transactions"] = m_sponsoredTransactions;
    j["sponsor"] = m_sponsor;
}

}
