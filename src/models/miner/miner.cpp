#include "pch.h"
#include "miner.h"

namespace logpass {

Miner_ptr Miner::clone(uint32_t blockId) const
{
    Miner_ptr newMiner = std::shared_ptr<Miner>(new Miner(*this));
    newMiner->iteration = iteration + 1;
    newMiner->committedIn = blockId;
    return newMiner;
}

Miner_ptr Miner::create(const MinerId& minerId, const UserId& owner, uint32_t blockId)
{
    Miner_ptr miner(new Miner);
    miner->id = minerId;
    miner->owner = owner;
    miner->committedIn = blockId;
    return miner;
}

Miner_cptr Miner::load(Serializer& s, uint32_t /*blockId*/)
{
    Miner_ptr miner(new Miner);
    s(*miner);
    return miner;
}

void Miner::serialize(Serializer& s)
{
    s(version);
    if (version != 1) {
        THROW_SERIALIZER_EXCEPTION("Invalid Miner version");
    }

    s(id);
    s(owner);
    s(iteration);
    s(committedIn);
    s(stake);
    s(lockedStake);
    s(lockedStakeBuckets);
    s(lastStakeUpdate);
    s(settings);
    s(banned);
}

void Miner::addStake(uint64_t stake, bool fromFee)
{
    if (fromFee) {
        lockedStakeBuckets[0] += stake;
    } else {
        lockedStakeBuckets[lockedStakeBuckets.size() - 2] += stake;
    }
    this->stake += stake;
    this->lockedStake += stake;
}

void Miner::withdrawStake(uint64_t unlockedStake, uint64_t lockedStake)
{
    ASSERT(unlockedStake <= (this->stake - this->lockedStake));
    ASSERT(lockedStake <= this->lockedStake);
    ASSERT(unlockedStake + lockedStake <= this->stake);

    this->stake -= (unlockedStake + lockedStake);
    this->lockedStake -= lockedStake;

    // remove enough stake from locked stake buckets
    for (auto& bucket : lockedStakeBuckets) {
        if (bucket >= lockedStake) {
            bucket -= lockedStake;
            break;
        } else {
            lockedStake -= bucket;
            bucket = 0;
        }
    }
}

void Miner::unlockStake(uint32_t blockId)
{
    if (blockId / kBlocksPerDay <= lastStakeUpdate / kBlocksPerDay) {
        return;
    }

    lastStakeUpdate = blockId;
    lockedStake -= lockedStakeBuckets.back();
    lockedStakeBuckets.back() = 0;
    std::rotate(lockedStakeBuckets.rbegin(), lockedStakeBuckets.rbegin() + 1, lockedStakeBuckets.rend());
}

uint64_t Miner::getActiveStake(uint32_t currentBlockId) const
{
    return stake;
}

void Miner::toJSON(json& j) const
{
    j["id"] = getId();
    j["owner"] = owner;
    j["iteration"] = iteration;
    j["committed_in"] = committedIn;
    j["stake"] = stake;
    j["locked_stake"] = lockedStake;
    j["locked_stake_buckets"] = lockedStakeBuckets;
    j["last_stake_update"] = lastStakeUpdate;
    j["settings"] = settings;
    j["banned"] = banned;
}

}
