#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/storage/update_prefix.h>
#include "../transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_AUTO_TEST_SUITE(storage);
BOOST_FIXTURE_TEST_SUITE(update_prefix, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    createUser();
    createUser();
    createPrefix("PREFIX", userIds[0]);

    PrefixSettings settings;
    settings.allowedUsers = { userIds[1] };

    auto transaction = StorageUpdatePrefixTransaction::create(1, -1, "PREFIX", settings)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    auto prefix = db->unconfirmed().storage.getPrefix("PREFIX");
    BOOST_TEST_REQUIRE(prefix->getId() == "PREFIX");
    BOOST_TEST_REQUIRE(prefix->owner == userIds[0]);
    BOOST_TEST_REQUIRE(prefix->settings.allowedUsers.size() == 1);
    BOOST_TEST_REQUIRE(prefix->settings.allowedUsers.contains(userIds[1]));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee());
}

BOOST_AUTO_TEST_CASE(invalid_prefix_owner)
{
    createUser();
    createUser();
    createPrefix("PREFIX", userIds[0]);

    PrefixSettings settings;
    settings.allowedUsers = { userIds[1] };

    auto transaction = StorageUpdatePrefixTransaction::create(1, -1, "PREFIX", settings)->
        setUserId(userIds[1])->sign({ keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
