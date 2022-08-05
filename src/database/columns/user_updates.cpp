#include "pch.h"
#include "user_updates.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions UserUpdatesColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    return columnOptions;
}

void UserUpdatesColumn::addUpdatedUserId(uint32_t blockId, const UserId& userId)
{
    std::unique_lock lock(m_mutex);
    m_userIdsPerBlock[blockId].insert(userId);
    ASSERT(blockId > state().blockId);
}

uint32_t UserUpdatesColumn::getUpdatedUserIdsCount(uint32_t blockId, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_userIdsPerBlock.find(blockId);
        if (it != m_userIdsPerBlock.end()) {
            return it->second.size();
        }
    }

    auto s = get<uint32_t>(boost::endian::endian_reverse(blockId));
    if (!s) {
        return 0;
    }

    return s->get<uint32_t>();
}

std::set<UserId> UserUpdatesColumn::getUpdatedUserIds(uint32_t blockId, uint32_t page, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_userIdsPerBlock.find(blockId);
        if (it != m_userIdsPerBlock.end()) {
            uint32_t lastEntry = std::min<uint32_t>(it->second.size(), (page + 1) * PAGE_SIZE);
            return std::set<UserId>(std::next(it->second.begin(), page * PAGE_SIZE),
                                    std::next(it->second.begin(), lastEntry));
        }
    }

    std::set<UserId> ret;
    auto s = get<uint32_t, uint32_t>(boost::endian::endian_reverse(blockId), page);
    if (!s) {
        return ret;
    }
    (*s)(ret);
    return ret;
}

void UserUpdatesColumn::load()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::load();
}

void UserUpdatesColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    // serialize blocks
    for (auto& [blockIdLE, userIdsSet] : m_userIdsPerBlock) {
        // big endian blockId
        uint32_t blockIdBE = boost::endian::endian_reverse(blockIdLE);

        // user ids count
        Serializer sUserIdsCount;
        sUserIdsCount.put<uint32_t>(userIdsSet.size());
        put<uint32_t>(batch, blockIdBE, sUserIdsCount);

        // user ids
        std::vector<UserId> userIds(userIdsSet.begin(), userIdsSet.end());
        for (uint32_t page = 0; page < (userIds.size() + PAGE_SIZE - 1) / PAGE_SIZE; ++page) {
            uint32_t lastEntry = std::min<uint32_t>(userIds.size(), (page + 1) * PAGE_SIZE);
            std::set<UserId> pageUserIds(std::next(userIds.begin(), page * PAGE_SIZE),
                                         std::next(userIds.begin(), lastEntry));
            Serializer sVal;
            sVal(pageUserIds);
            put<uint32_t, uint32_t>(batch, blockIdBE, page, sVal);
        }
    }
}

void UserUpdatesColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_userIdsPerBlock.clear();
}

void UserUpdatesColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_userIdsPerBlock.clear();
}

}
}
