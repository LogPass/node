#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/withdraw_stake.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(withdraw_stake, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    createUser();
    auto miner = createMiner(userIds[0])->clone(1);
    miner->stake = 1000;
    miner->addStake(1000, false);
    updateMiner(miner);

    auto transaction = WithdrawStakeTransaction::create(1, -1, miners[0]->getId(), 500, 500)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(miners[0]->stake == 1000);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == 500);
    BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[kStakingDuration - 2] == 500);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost() + 975);
}

BOOST_AUTO_TEST_CASE(multiple_locked_buckets)
{
    createUser();
    auto miner = createMiner(userIds[0])->clone(1);
    miner->stake = kStakingDuration;
    miner->lockedStake = kStakingDuration;
    std::fill(miner->lockedStakeBuckets.begin(), miner->lockedStakeBuckets.end(), 1);
    updateMiner(miner);

    auto transaction = WithdrawStakeTransaction::create(1, -1, miners[0]->getId(), 0, 20)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(miners[0]->stake == kStakingDuration - 20);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == kStakingDuration - 20);
    for (size_t i = 0; i < 20; ++i) {
        BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[i] == 0);
    }
    for (size_t i = 20; i < kStakingDuration; ++i) {
        BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[i] == 1);
    }
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost() + 19);
}

BOOST_AUTO_TEST_CASE(invalid_values)
{
    createUser();
    auto miner = createMiner(userIds[0])->clone(1);
    miner->stake = 1000;
    miner->addStake(1000, false);
    updateMiner(miner);

    auto transaction1 = WithdrawStakeTransaction::create(1, -1, miners[0]->getId(), 0, 0)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 1), TransactionValidationError);
    auto transaction2 = WithdrawStakeTransaction::create(1, -1, miners[0]->getId(), 1001, 0)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 1), TransactionValidationError);
    auto transaction3 = WithdrawStakeTransaction::create(1, -1, miners[0]->getId(), 0, 1001)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction3, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(max_values)
{
    createUser();
    auto miner = createMiner(userIds[0])->clone(1);
    miner->stake = 1000;
    miner->addStake(1000, false);
    updateMiner(miner);

    auto transaction = WithdrawStakeTransaction::create(1, -1, miners[0]->getId(), 1000, 1000)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    BOOST_TEST_REQUIRE(miners[0]->stake == 0);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == 0);
    for (size_t i = 0; i < kStakingDuration; ++i) {
        BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets[i] == 0);
    }
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee() - transaction->getCost() + 1950);
}

BOOST_AUTO_TEST_CASE(invalid_miner_owner)
{
    createUser();
    createUser();
    auto miner = createMiner(userIds[1])->clone(1);
    miner->stake = 1000;
    miner->addStake(1000, false);
    updateMiner(miner);

    auto transaction = WithdrawStakeTransaction::create(1, -1, miners[0]->getId(), 500, 500)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
