#include "pch.h"
#include "miners.h"

namespace logpass {
namespace database {

Miner_cptr MinersFacade::getMiner(const MinerId& minerId) const
{
    return m_miners->getMiner(minerId, m_confirmed);
}

Miner_cptr MinersFacade::getRandomMiner() const
{
    return m_miners->getRandomMiner(m_confirmed);
}

void MinersFacade::addMiner(const Miner_cptr& miner)
{
    m_miners->addMiner(miner);
}

void MinersFacade::updateMiner(const Miner_cptr& miner)
{
    m_miners->updateMiner(miner);
}

uint64_t MinersFacade::getMinersCount() const
{
    return m_miners->getMinersCount(m_confirmed);
}

uint64_t MinersFacade::getStakedTokens() const
{
    return m_miners->getStakedTokens(m_confirmed);
}

TopMinersSet MinersFacade::getTopMiners() const
{
    return m_miners->getTopMiners(m_confirmed);
}

std::map<MinerId, Endpoint> MinersFacade::getMinerEndpoints() const
{
    return m_miners->getMinerEndpoints(m_confirmed);
}

}
}
