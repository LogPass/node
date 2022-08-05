#pragma once

#include "facade.h"

#include <database/columns/users.h>
#include <database/columns/user_history.h>
#include <database/columns/user_sponsors.h>
#include <database/columns/user_updates.h>
#include <models/user/user.h>

namespace logpass {
namespace database {

class UsersFacade : public Facade {
public:
    UsersFacade(UsersColumn* users, UserHistoryColumn* userHistory, UserSponsorsColumn* userSponsors,
                UserUpdatesColumn* userUpdates, bool confirmed) :
        m_users(users), m_userHistory(userHistory), m_userSponsors(userSponsors), m_userUpdates(userUpdates),
        m_confirmed(confirmed)
    {}

    UsersFacade(const std::unique_ptr<UsersColumn>& users, const std::unique_ptr<UserHistoryColumn>& userHistory,
                const std::unique_ptr<UserSponsorsColumn>& userSponsors,
                const std::unique_ptr<UserUpdatesColumn>& userUpdates, bool confirmed) :
        UsersFacade(users.get(), userHistory.get(), userSponsors.get(), userUpdates.get(), confirmed)
    {}

    User_cptr getUser(const UserId& userId) const;
    User_cptr getRandomUser() const;
    void addUser(const User_cptr& user);
    void updateUser(const User_cptr& user);

    void preloadUser(const UserId& userId);

    uint64_t getUsersCount() const;
    uint64_t getTokens() const;

    // user history
    std::vector<UserHistory> getUserHistory(const UserId& userId, uint32_t page) const;
    void addUserHistory(const UserId& userId, uint32_t page, const UserHistory& history);

    // user sponsors
    std::vector<UserSponsor> getUserSponsors(const UserId& userId, uint32_t page) const;
    void addUserSponsor(const UserId& userId, uint32_t page, const UserSponsor& history);

    // user updates
    uint32_t getUpdatedUserIdsCount(uint32_t blockId) const;
    std::set<UserId> getUpdatedUserIds(uint32_t blockId, uint32_t page) const;

private:
    UsersColumn* m_users;
    UserHistoryColumn* m_userHistory;
    UserSponsorsColumn* m_userSponsors;
    UserUpdatesColumn* m_userUpdates;
    bool m_confirmed;
};

}
}
