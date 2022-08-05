#include "pch.h"
#include "users.h"

namespace logpass {
namespace database {

User_cptr UsersFacade::getUser(const UserId& userId) const
{
    return m_users->getUser(userId, m_confirmed);
}

User_cptr UsersFacade::getRandomUser() const
{
    return m_users->getRandomUser(m_confirmed);
}

void UsersFacade::addUser(const User_cptr& user)
{
    ASSERT(user->getId().isValid());
    m_users->addUser(user);
    m_userUpdates->addUpdatedUserId(user->committedIn, user->getId());
}

void UsersFacade::updateUser(const User_cptr& user)
{
    ASSERT(user->getId().isValid());
    m_users->updateUser(user);
    m_userUpdates->addUpdatedUserId(user->committedIn, user->getId());
}

void UsersFacade::preloadUser(const UserId& userId)
{
    m_users->preloadUser(userId);
}

uint64_t UsersFacade::getUsersCount() const
{
    return m_users->getUsersCount(m_confirmed);
}

uint64_t UsersFacade::getTokens() const
{
    return m_users->getTokens(m_confirmed);
}

std::vector<UserHistory> UsersFacade::getUserHistory(const UserId& userId, uint32_t page) const
{
    return m_userHistory->getUserHistory(userId, page, m_confirmed);
}

void UsersFacade::addUserHistory(const UserId& userId, uint32_t page, const UserHistory& history)
{
    m_userHistory->addUserHistory(userId, page, history);
}

std::vector<UserSponsor> UsersFacade::getUserSponsors(const UserId& userId, uint32_t page) const
{
    return m_userSponsors->getUserSponsors(userId, page, m_confirmed);
}

void UsersFacade::addUserSponsor(const UserId& userId, uint32_t page, const UserSponsor& history)
{
    m_userSponsors->addUserSponsor(userId, page, history);
}

uint32_t UsersFacade::getUpdatedUserIdsCount(uint32_t blockId) const
{
    return m_userUpdates->getUpdatedUserIdsCount(blockId, m_confirmed);
}

std::set<UserId> UsersFacade::getUpdatedUserIds(uint32_t blockId, uint32_t page) const
{
    return m_userUpdates->getUpdatedUserIds(blockId, page, m_confirmed);
}

}
}
