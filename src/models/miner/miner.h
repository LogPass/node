#pragma once

#include "miner_settings.h"

namespace logpass {

struct Miner;
using Miner_ptr = std::shared_ptr<Miner>;
using Miner_cptr = std::shared_ptr<const Miner>;

using MinersQueue = std::deque<MinerId>;
using MinersQueue_ptr = std::shared_ptr<MinersQueue>;
using MinersQueue_cptr = std::shared_ptr<const MinersQueue>;

struct Miner {
    uint8_t version = 1;
    MinerId id;
    UserId owner;
    uint64_t iteration = 0;
    uint32_t committedIn = 0;

    uint64_t stake = 0;
    uint64_t lockedStake = 0;
    std::array<uint64_t, kStakingDuration> lockedStakeBuckets = {};
    uint32_t lastStakeUpdate = 0;

    MinerSettings settings;
    uint8_t banned = 0;


protected:
    Miner() = default;
    Miner(const Miner&) = default;
    Miner& operator = (const Miner&) = delete;

public:
    Miner_ptr clone(uint32_t blockId) const;
    void serialize(Serializer& s);

    // creates new miner
    static Miner_ptr create(const MinerId& minerId, const UserId& owner, uint32_t blockId);
    // loads miner
    static Miner_cptr load(Serializer& s, uint32_t blockId = 0);

    // for containers
    MinerId getId() const
    {
        return id;
    }

    //
    void addStake(uint64_t stake, bool fromFee = false);
    // 
    void withdrawStake(uint64_t unlockedStake, uint64_t lockedStake);
    //
    void unlockStake(uint32_t blockId);

    uint64_t getActiveStake(uint32_t currentBlockId) const;

    void toJSON(json& j) const;
};

struct MinersCompare {
    bool operator()(const Miner_cptr& first, const Miner_cptr& second) const
    {
        if (first->stake == second->stake) {
            return first->getId() < second->getId();
        }
        return first->stake > second->stake;
    }
};

using TopMinersSet = std::set<Miner_cptr, MinersCompare>;

}
