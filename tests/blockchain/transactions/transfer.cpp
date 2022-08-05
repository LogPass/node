#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/transfer.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(transfer, TransactionFixture);

BOOST_AUTO_TEST_CASE(execution)
{
    createUser();
    createUser();

    auto transaction = TransferTransaction::create(1, -1, userIds[1], 1000)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));

    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee - 1000);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance + 1000);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[1], 0).size() == 1);
}

BOOST_AUTO_TEST_CASE(staked_execution)
{
    auto user = createUser()->clone(1);
    auto miner = createMiner(user->getId());
    user->miner = miner->getId();
    updateUser(user);
    createUser();

    auto transaction = TransferTransaction::create(1, 1, userIds[1], 1000)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));

    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee * 20 - 1000);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance + 1000);
    BOOST_TEST_REQUIRE(miners[0]->stake == kTransactionFee * 20);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == kTransactionFee * 20);
    BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[0] == kTransactionFee * 20);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[1], 0).size() == 1);
}

BOOST_AUTO_TEST_CASE(sponsored)
{
    createUser();
    createUser();
    createUser();

    auto transaction = TransferTransaction::create(1, -1, userIds[2], kTestUserBalance)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));

    BOOST_TEST_REQUIRE(users[0]->tokens == 0);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - kTransactionFee);
    BOOST_TEST_REQUIRE(users[2]->tokens == kTestUserBalance * 2);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[1], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[2], 0).size() == 1);
}

BOOST_AUTO_TEST_CASE(sponsored_invalid)
{
    createUser();
    createUser();
    createUser();
    createUser({
        .tokens = 0
    });

    auto transaction1 = TransferTransaction::create(1, -1, userIds[2], kTestUserBalance + 1)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 1), TransactionValidationError);

    auto transaction2 = TransferTransaction::create(1, -1, userIds[2], kTestUserBalance / 2)->
        setUserId(userIds[0])->setSponsorId(userIds[3])->sign({ keys[0], keys[1], keys[3] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
