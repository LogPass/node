#include "pch.h"

#include <boost/test/unit_test.hpp>

#include <models/miner/miner.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(models);
BOOST_AUTO_TEST_SUITE(miner);

BOOST_AUTO_TEST_CASE(locked_stake)
{
    auto miner = Miner::create(PublicKey::generateRandom(), PublicKey::generateRandom(), 1);
    for (size_t i = 0; i < miner->lockedStakeBuckets.size(); ++i) {
        miner->lockedStakeBuckets[i] = i + 1;
        miner->lockedStake += i + 1;
        miner->stake += i + 1;
    }
    miner->lastStakeUpdate = 1;
    miner->unlockStake(1);
    BOOST_TEST_REQUIRE(miner->stake == miner->lockedStake);
    miner->unlockStake(1 + kBlocksPerDay);
    BOOST_TEST_REQUIRE(miner->stake == miner->lockedStake + miner->lockedStakeBuckets.size());
    BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[0] == 0);
    for (size_t i = 1; i < miner->lockedStakeBuckets.size(); ++i) {
        BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[i] == i);
    }
    miner->unlockStake(1 + kBlocksPerDay * 2);
    BOOST_TEST_REQUIRE(miner->stake == miner->lockedStake + miner->lockedStakeBuckets.size() * 2 - 1);
    BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[0] == 0);
    BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[1] == 0);
    for (size_t i = 2; i < miner->lockedStakeBuckets.size(); ++i) {
        BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[i] == i - 1);
    }
    size_t stake = miner->stake;
    miner->addStake(1000, true);
    BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[0] == 1000);
    BOOST_TEST_REQUIRE(miner->stake == stake + 1000);
    BOOST_TEST_REQUIRE(miner->stake == miner->lockedStake + miner->lockedStakeBuckets.size() * 2 - 1);
    miner->unlockStake(1 + kBlocksPerDay * 3);
    BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[0] == 0);
    BOOST_TEST_REQUIRE(miner->lockedStakeBuckets[1] == 1000);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
