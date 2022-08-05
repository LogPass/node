#include "pch.h"
#include "miners.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions MinersColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    return columnOptions;
}

Miner_cptr MinersColumn::getMiner(const MinerId& minerId, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_miners.find(minerId);
        if (it != m_miners.end()) {
            return it->second;
        }
    }

    Serializer_ptr s = get<MinerId>(minerId);
    if (!s) {
        return nullptr;
    }
    return Miner::load(*s, state(confirmed).blockId);
}

Miner_cptr MinersColumn::getRandomMiner(bool confirmed) const
{
    MinerId randomMinerId(PublicKey::generateRandom());

    rocksdb::Iterator* it = m_db->NewIterator(rocksdb::ReadOptions(), m_handle);
    it->Seek(randomMinerId);
    if (!it->Valid()) {
        it->SeekForPrev(randomMinerId);
    }
    if (!it->Valid() || it->key().empty()) {
        delete it;
        if (!confirmed) {
            std::shared_lock lock(m_mutex);
            if (!m_miners.empty()) {
                return m_miners.begin()->second;
            }
        }
        return nullptr;
    }

    Serializer s(it->value());
    auto miner = Miner::load(s, state(confirmed).blockId);
    delete it;
    return miner;
}

void MinersColumn::addMiner(const Miner_cptr& miner)
{
    ASSERT(!getMiner(miner->getId(), false));
    ASSERT(miner->committedIn > 0 && miner->iteration == 0);
    std::unique_lock lock(m_mutex);
    m_miners[miner->getId()] = miner;

    // update miners count
    state().miners += 1;
    state().stakedTokens += miner->stake;

    // update top miners
    auto& topMiners = state().topMiners;
    topMiners.insert(miner);
    while (topMiners.size() > TOP_MINERS_SIZE) {
        topMiners.erase(std::prev(topMiners.end()));
    }

    // update miner endpoints
    auto& minerEndpoints = state().minerEndpoints;
    if (miner->settings.endpoint.isValid() &&
        (minerEndpoints.size() < MINER_ENDPOINTS_SIZE || miner->stake >= MINER_ENDPOINTS_MINIMUM_STAKE)) {
        minerEndpoints[miner->getId()] = miner->settings.endpoint;
    }
}

void MinersColumn::updateMiner(const Miner_cptr& miner)
{
    auto oldMiner = getMiner(miner->getId(), false);
    ASSERT(miner->committedIn >= oldMiner->committedIn && miner->iteration == oldMiner->iteration + 1);
    std::unique_lock lock(m_mutex);
    m_miners[miner->getId()] = miner;

    // updated staked tokens
    state().stakedTokens -= oldMiner->stake;
    state().stakedTokens += miner->stake;

    // remove miner from top miners
    auto& topMiners = state().topMiners;
    topMiners.erase(oldMiner);

    // update top miners
    topMiners.insert(miner);
    while (topMiners.size() > TOP_MINERS_SIZE) {
        topMiners.erase(std::prev(topMiners.end()));
    }

    // update miner endpoints
    auto& minerEndpoints = state().minerEndpoints;
    if (miner->settings.endpoint.isValid() &&
        (minerEndpoints.size() < MINER_ENDPOINTS_SIZE || miner->stake >= MINER_ENDPOINTS_MINIMUM_STAKE)) {
        minerEndpoints[miner->getId()] = miner->settings.endpoint;
    } else {
        minerEndpoints.erase(miner->getId());
    }
}

uint64_t MinersColumn::getMinersCount(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).miners;
}

uint64_t MinersColumn::getStakedTokens(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).stakedTokens;
}

TopMinersSet MinersColumn::getTopMiners(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).topMiners;
}

std::map<MinerId, Endpoint> MinersColumn::getMinerEndpoints(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).minerEndpoints;
}

void MinersColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_miners.empty());
    StatefulColumn::load();
}

void MinersColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [minerId, miner] : m_miners) {
        Serializer s;
        s(miner);
        put<MinerId>(batch, minerId, s);
    }
}

void MinersColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_miners.clear();
}

void MinersColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_miners.clear();
}

}
}
