#pragma once

#include "facade.h"

#include <database/columns/miners.h>
#include <models/miner/miner.h>

namespace logpass {
namespace database {

class MinersFacade : public Facade {
public:
    MinersFacade(MinersColumn* miners, bool confirmed) : m_miners(miners), m_confirmed(confirmed) {}

    MinersFacade(const std::unique_ptr<MinersColumn>& miners, bool confirmed) :
        MinersFacade(miners.get(), confirmed)
    {}

    Miner_cptr getMiner(const MinerId& minerId) const;

    Miner_cptr getRandomMiner() const;

    void addMiner(const Miner_cptr& miner);

    void updateMiner(const Miner_cptr& miner);

    uint64_t getMinersCount() const;

    uint64_t getStakedTokens() const;

    TopMinersSet getTopMiners() const;

    std::map<MinerId, Endpoint> getMinerEndpoints() const;

private:
    MinersColumn* m_miners;
    bool m_confirmed;
};

}
}
