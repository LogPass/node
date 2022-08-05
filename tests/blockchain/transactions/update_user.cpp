#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/update_user.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(update_user, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    auto user = createUser()->clone(1);
    keys.push_back(PrivateKey::generate());
    UserSettings newSettings = user->settings;
    newSettings.keys = UserKeys::create({ { keys[0].publicKey(), 1 }, { keys[1].publicKey(), 1 } });

    auto transaction = UpdateUserTransaction::create(1, -1, newSettings)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->getSettings().keys.size() == 1);
    db->commit(2);
    refresh();
    BOOST_TEST_REQUIRE(users[0]->getSettings().keys.size() == 2);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
