#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/increase_stake.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(increase_stake, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    createUser();
    createMiner(userIds[0]);

    auto transaction = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), 1000)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(miners[0]->stake == 1000);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == 1000);
    BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[kStakingDuration - 2] == 1000);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost());
}

BOOST_AUTO_TEST_CASE(max_tokens)
{
    createUser();
    createMiner(userIds[0]);

    uint64_t value = kTestUserBalance - kTransactionFee;
    auto transaction = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), value)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(miners[0]->stake == value);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == value);
    BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[kStakingDuration - 2] == value);
    BOOST_TEST_REQUIRE(users[0]->tokens == 0);
}

BOOST_AUTO_TEST_CASE(sponsored_execution)
{
    createUser();
    createUser();
    createMiner(userIds[0]);

    auto transaction = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), 1000)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(miners[0]->stake == 1000);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == 1000);
    BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[kStakingDuration - 2] == 1000);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getCost());
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - transaction->getFee());
}

BOOST_AUTO_TEST_CASE(sponsored_execution_with_max_tokens)
{
    createUser();
    createUser();
    createMiner(userIds[0]);

    auto transaction = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), kTestUserBalance)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(miners[0]->stake == kTestUserBalance);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == kTestUserBalance);
    BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[kStakingDuration - 2] == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->tokens == 0);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - transaction->getFee());
}

BOOST_AUTO_TEST_CASE(invalid_value)
{
    createUser();
    createMiner(userIds[0]);
    uint64_t value = kTestUserBalance - kTransactionFee + 1;
    auto transaction1 = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), value)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 1), TransactionValidationError);
    auto transaction2 = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), 0)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(invalid_value_sponsored)
{
    createUser();
    createUser();
    createMiner(userIds[0]);
    uint64_t value = kTestUserBalance + 1;
    auto transaction1 = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), value)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 1), TransactionValidationError);
    auto transaction2 = IncreaseStakeTransaction::create(1, -1, miners[0]->getId(), 0)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
