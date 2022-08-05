#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/select_miner.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(select_miner, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    createUser();
    createMiner(userIds[0]);

    auto transaction = SelectMinerTransaction::create(1, -1, miners[0]->getId())->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(users[0]->miner == miners[0]->getId());
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost());
}

BOOST_AUTO_TEST_CASE(invalid_miner)
{
    createUser();
    MinerId minerId(PublicKey::generateRandom());

    auto transaction = SelectMinerTransaction::create(1, -1, minerId)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
