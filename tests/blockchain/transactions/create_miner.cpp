#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/create_miner.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(create_miner, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    createUser();
    MinerId minerId(PublicKey::generateRandom());

    auto transaction = CreateMinerTransaction::create(1, -1, minerId)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    auto miner = db->unconfirmed().miners.getMiner(minerId);
    BOOST_TEST_REQUIRE(miner->getId() == minerId);
    BOOST_TEST_REQUIRE(miner->owner == userIds[0]);
    BOOST_TEST_REQUIRE(miner->stake == 0);
    BOOST_TEST_REQUIRE(miner->lockedStake == 0);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee());
}

BOOST_AUTO_TEST_CASE(duplicated_miner)
{
    createUser();
    MinerId minerId(PublicKey::generateRandom());

    auto transaction1 = CreateMinerTransaction::create(1, -1, minerId)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction1, 1));
    auto transaction2 = CreateMinerTransaction::create(1, -2, minerId)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 2), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
