#pragma once

#include "stateful_column.h"
#include <models/miner/miner.h>

namespace logpass {
namespace database {

struct MinersColumnState : public ColumnState {
    uint64_t miners = 0;
    uint64_t stakedTokens = 0;
    TopMinersSet topMiners;
    std::map<MinerId, Endpoint> minerEndpoints;

    void serialize(Serializer& s)
    {
        ColumnState::serialize(s);
        s(miners);
        s(stakedTokens);
        s(topMiners);
        s(minerEndpoints);
    }
};

// keeps miners
class MinersColumn : public StatefulColumn<MinersColumnState> {
public:
    constexpr static size_t TOP_MINERS_SIZE = kMinersQueueSize * 2;
    constexpr static size_t MINER_ENDPOINTS_SIZE = 10'000; // can be max 2 times bigger
    constexpr static size_t MINER_ENDPOINTS_MINIMUM_STAKE = kTotalNumberOfTokens / MINER_ENDPOINTS_SIZE;

    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return "miners";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    Miner_cptr getMiner(const MinerId& minerId, bool confirmed) const;
    Miner_cptr getRandomMiner(bool confirmed) const;
    void addMiner(const Miner_cptr& miner);
    void updateMiner(const Miner_cptr& miner);
    uint64_t getStakedTokens(bool confirmed) const;

    uint64_t getMinersCount(bool confirmed) const;
    TopMinersSet getTopMiners(bool confirmed) const;
    std::map<MinerId, Endpoint> getMinerEndpoints(bool confirmed) const;

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    // changed miners
    std::map<MinerId, Miner_cptr> m_miners;
};

}
}
