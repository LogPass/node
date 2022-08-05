#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/blockchain.h>

using namespace logpass;

struct MinersQueueFixture {
    MinersQueue queue;
    std::map<MinerId, int> occurrences;

    std::map<MinerId, int> countOccurrences(const MinersQueue& queue)
    {
        std::map<MinerId, int> occurrences;
        for (auto& miner : queue) {
            occurrences.emplace(miner, 0).first->second += 1;
        }
        return occurrences;
    }

    std::vector<Miner_ptr> createMiners(const std::vector<uint64_t>& stakes)
    {
        std::vector<Miner_ptr> miners;
        for (auto& stake : stakes) {
            auto miner = Miner::create(MinerId(PublicKey::generateRandom()), UserId(), 1);
            miner->stake = stake;
            miners.push_back(miner);
        }
        return miners;
    }

    std::vector<Miner_ptr> createMiners(const std::vector<std::map<uint32_t, uint64_t>>& stakesHistory)
    {
        std::vector<Miner_ptr> miners;
        for (auto& stakeHistory : stakesHistory) {
            auto miner = Miner::create(MinerId(PublicKey::generateRandom()), UserId(), 1);
            miner->stake = stakeHistory.rbegin()->second;
            miners.push_back(miner);
        }
        return miners;
    }

    MinersQueue getNextMiners(const MinersQueue& currentMinersQueue, const std::vector<Miner_ptr>& miners,
                              uint32_t newMiners, uint32_t currentBlockId)
    {
        return Blockchain::getNextMiners(currentMinersQueue, TopMinersSet(miners.begin(), miners.end()),
                                         newMiners, currentBlockId);
    }
};

BOOST_FIXTURE_TEST_SUITE(mining_queue, MinersQueueFixture);

BOOST_AUTO_TEST_CASE(two_miners)
{
    auto miners = createMiners({ 2000, 10000 });
    queue = getNextMiners({}, miners, 240, 1);
    occurrences = countOccurrences(queue);
    BOOST_TEST_REQUIRE(occurrences[miners[0]->getId()] == 60);
    BOOST_TEST_REQUIRE(occurrences[miners[1]->getId()] == 180);

    miners = createMiners({ 2000, 1 });
    queue = getNextMiners({}, miners, 240, 1);
    occurrences = countOccurrences(queue);
    BOOST_TEST_REQUIRE(occurrences[miners[0]->getId()] == 240);

    miners = createMiners({ 10000, 10000 });
    queue = getNextMiners({}, miners, 240, 1);
    occurrences = countOccurrences(queue);
    BOOST_TEST_REQUIRE(occurrences[miners[0]->getId()] == 120);
    BOOST_TEST_REQUIRE(occurrences[miners[1]->getId()] == 120);

    miners = createMiners({ 5000, 10000 });
    queue = getNextMiners({}, miners, 240, 1);
    occurrences = countOccurrences(queue);
    BOOST_TEST_REQUIRE(occurrences[miners[0]->getId()] == 96);
    BOOST_TEST_REQUIRE(occurrences[miners[1]->getId()] == 144);

    miners = createMiners({ 4999, 10000 });
    queue = getNextMiners({}, miners, 240, 1);
    occurrences = countOccurrences(queue);
    BOOST_TEST_REQUIRE(occurrences[miners[0]->getId()] == 80);
    BOOST_TEST_REQUIRE(occurrences[miners[1]->getId()] == 160);

    miners = createMiners({ 3334, 10000 });
    queue = getNextMiners({}, miners, 240, 1);
    occurrences = countOccurrences(queue);
    BOOST_TEST_REQUIRE(occurrences[miners[0]->getId()] == 80);
    BOOST_TEST_REQUIRE(occurrences[miners[1]->getId()] == 160);

    miners = createMiners({ 3333, 10000 });
    queue = getNextMiners({}, miners, 240, 1);
    occurrences = countOccurrences(queue);
    BOOST_TEST_REQUIRE(occurrences[miners[0]->getId()] == 69);
    BOOST_TEST_REQUIRE(occurrences[miners[1]->getId()] == 171);
}

BOOST_AUTO_TEST_CASE(three_miners)
{
    auto topMiners = createMiners({ 5, 10, 15 });

    queue = getNextMiners({}, topMiners, 240, 1);
    for (auto occurrences = countOccurrences(queue); auto & miner : topMiners) {
        BOOST_TEST(occurrences[miner->id] == (240 / 30) * miner->stake);
    }

    queue = getNextMiners(queue, topMiners, 30, 1);
    for (auto occurrences = countOccurrences(queue); auto& miner : topMiners) {
        BOOST_TEST(occurrences[miner->id] == miner->stake);
    }
}

BOOST_AUTO_TEST_CASE(eight_miners)
{
    auto topMiners = createMiners({ 10, 10, 20, 20, 20, 40, 40, 80 });

    queue = getNextMiners({}, topMiners, 240, 1);
    for (auto occurrences = countOccurrences(queue); auto & miner : topMiners) {
        BOOST_TEST(occurrences[miner->id] == miner->stake);
    }

    queue = getNextMiners({}, topMiners, 24, 1);
    for (auto occurrences = countOccurrences(queue); auto & miner : topMiners) {
        BOOST_TEST(occurrences[miner->id] == miner->stake / 10);
    }
}

BOOST_AUTO_TEST_SUITE_END();
