#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <database/facades/users.h>
#include <database/database_fixture.h>

using namespace logpass;
using namespace logpass::database;

class UsersFacadeDatabase : public BaseDatabase {
    friend class SharedThread<UsersFacadeDatabase>;
public:
    using BaseDatabase::BaseDatabase;

    void start() override
    {
        BaseDatabase::start({
            { UsersColumn::getName(), UsersColumn::getOptions() },
            { UserHistoryColumn::getName(), UserHistoryColumn::getOptions() },
            { UserSponsorsColumn::getName(), UserSponsorsColumn::getOptions() },
            { UserUpdatesColumn::getName(), UserUpdatesColumn::getOptions() }
                            });
        usersColumn = std::make_unique<UsersColumn>(m_db, m_handles[0]);
        userHistoryColumn = std::make_unique<UserHistoryColumn>(m_db, m_handles[1]);
        userSponsorsColumn = std::make_unique<UserSponsorsColumn>(m_db, m_handles[2]);
        userUpdatesColumn = std::make_unique<UserUpdatesColumn>(m_db, m_handles[3]);

        load();
    }

    void stop() override
    {
        BaseDatabase::stop();
    }

    UsersFacade users(bool confirmed = false)
    {
        return UsersFacade(usersColumn, userHistoryColumn, userSponsorsColumn, userUpdatesColumn, confirmed);
    }

    std::vector<Column*> columns() const override
    {
        return {
            usersColumn.get(), userHistoryColumn.get(), userSponsorsColumn.get(), userUpdatesColumn.get()
        };
    }

    std::unique_ptr<UsersColumn> usersColumn;
    std::unique_ptr<UserHistoryColumn> userHistoryColumn;
    std::unique_ptr<UserSponsorsColumn> userSponsorsColumn;
    std::unique_ptr<UserUpdatesColumn> userUpdatesColumn;
};

BOOST_AUTO_TEST_SUITE(facades);
BOOST_FIXTURE_TEST_SUITE(users, DatabaseFixture<UsersFacadeDatabase>);

BOOST_AUTO_TEST_CASE(basic)
{
    auto key = PublicKey::generateRandom();
    User_ptr firstUser = User::create(key, UserId(), 1, 1000);
    db->users().addUser(firstUser);
    BOOST_TEST_REQUIRE(db->users(true).getUser(firstUser->id) == nullptr);
    BOOST_TEST_REQUIRE(db->users().getUser(firstUser->id) != nullptr);
    BOOST_TEST_REQUIRE(db->users(true).getUsersCount() == 0);
    BOOST_TEST_REQUIRE(db->users().getUsersCount() == 1);
    BOOST_TEST_REQUIRE(db->users().getTokens() == firstUser->tokens);
    db->commit(1);
    BOOST_TEST_REQUIRE(db->users(true).getUser(firstUser->id) != nullptr);
    BOOST_TEST_REQUIRE(db->users(true).getUsersCount() == 1);
    BOOST_TEST_REQUIRE(db->users(true).getTokens() == firstUser->tokens);
    auto user = db->users().getUser(firstUser->id)->clone(2);
    user->tokens = 2000;
    user->settings.keys = UserKeys::create(PublicKey::generateRandom());
    db->users().updateUser(user);
    db->commit(2);
    BOOST_TEST_REQUIRE(db->users(true).getUser(user->id) != nullptr);
    BOOST_TEST_REQUIRE(db->users(true).getUsersCount() == 1);
    BOOST_TEST_REQUIRE(db->users(true).getTokens() == user->tokens);
    user = db->users().getUser(user->id)->clone(4);
    auto userUpdate = std::make_shared<UserUpdate>();
    userUpdate->blockId = 5;
    userUpdate->settings.keys = UserKeys::create(key);
    user->pendingUpdate = userUpdate;
    db->users().updateUser(user);
    db->commit(4);
    user = db->users().getUser(user->id)->clone(5);
    BOOST_REQUIRE(user->settings != userUpdate->settings);
    db->commit(5);
    user = db->users().getUser(user->id)->clone(6);
    BOOST_REQUIRE(user->settings == userUpdate->settings);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
