#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/lock_user.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(lock_user, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 1);
    updateUser(user);
    auto transaction = LockUserTransaction::create(1, -1, { keys[0].publicKey() }, { userIds[1] })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 2)); 
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.size() == 1);
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.contains(keys[0].publicKey()));
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.size() == 1);
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.contains(userIds[1]));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost());
}

BOOST_AUTO_TEST_CASE(free_transaction_execution)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 1);
    updateUser(user);
    auto transaction = LockUserTransaction::create(1, 0, { keys[0].publicKey() }, { userIds[1] })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.size() == 1);
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.contains(keys[0].publicKey()));
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.size() == 1);
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.contains(userIds[1]));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions - 1);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
