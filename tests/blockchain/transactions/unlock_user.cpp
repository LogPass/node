#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/unlock_user.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(unlock_user, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 1);
    user->lockedSupervisors.insert(userIds[1]);
    user->lockedKeys.insert(keys[0].publicKey());
    updateUser(user);
    auto transaction = UnlockUserTransaction::create(1, -1, { keys[0].publicKey() }, { userIds[1] })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost());
}

BOOST_AUTO_TEST_CASE(free_transaction_execution)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 1);
    user->lockedSupervisors.insert(userIds[1]);
    user->lockedKeys.insert(keys[0].publicKey());
    updateUser(user);
    auto transaction = UnlockUserTransaction::create(1, 0, { keys[0].publicKey() }, { userIds[1] })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions - 1);
}

BOOST_AUTO_TEST_CASE(unlock_only_key)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 1);
    user->lockedSupervisors.insert(userIds[1]);
    user->lockedKeys.insert(keys[0].publicKey());
    updateUser(user);
    auto transaction = UnlockUserTransaction::create(1, -1, { keys[0].publicKey() }, {})->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.size() == 1);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost());
}

BOOST_AUTO_TEST_CASE(unlock_only_supervisor)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 1);
    user->lockedSupervisors.insert(userIds[1]);
    user->lockedKeys.insert(keys[0].publicKey());
    updateUser(user);
    auto transaction = UnlockUserTransaction::create(1, -1, {}, { userIds[1] })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.size() == 1);
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost());
}

BOOST_AUTO_TEST_CASE(partial_unlock)
{
    auto user = createUser()->clone(1);
    createUser();
    createUser();
    user->settings.supervisors = UserSupervisors::create({ { userIds[1], 1 }, { userIds[2], 1 } });
    user->settings.keys = UserKeys::create({ { keys[0].publicKey(), 1 }, { keys[1].publicKey(), 1 } });
    user->lockedSupervisors.insert(userIds[1]);
    user->lockedKeys.insert(keys[0].publicKey());
    updateUser(user);
    auto transaction = UnlockUserTransaction::create(1, -1, { keys[0].publicKey(), keys[1].publicKey() },
        { userIds[1], userIds[2] })->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->lockedKeys.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->lockedSupervisors.size() == 0);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost());
}

BOOST_AUTO_TEST_CASE(invalid_params)
{
    auto user = createUser()->clone(1);
    createUser();
    createUser();
    user->settings.supervisors = UserSupervisors::create({ { userIds[1], 1 }, { userIds[2], 1 } });
    user->lockedSupervisors.insert(userIds[1]);
    user->lockedKeys.insert(keys[0].publicKey());
    updateUser(user);

    auto randomKey = PublicKey::generateRandom();
    auto randomUserId = UserId(randomKey);

    auto transaction1 = UnlockUserTransaction::create(1, -1, {}, {})->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 1), TransactionValidationError);
    auto transaction2 = UnlockUserTransaction::create(1, -1, { randomKey }, {})->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 1), TransactionValidationError);
    auto transaction3 = UnlockUserTransaction::create(1, -1, {}, { randomUserId })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction3, 1), TransactionValidationError);
    auto transaction4 = UnlockUserTransaction::create(1, -1, { randomKey }, { randomUserId })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction4, 1), TransactionValidationError);
    auto transaction5 = UnlockUserTransaction::create(1, -1, { keys[0].publicKey(), randomKey }, { randomUserId })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction5, 1), TransactionValidationError);
    auto transaction6 = UnlockUserTransaction::create(1, -1, {}, { userIds[2] })->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction6, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
