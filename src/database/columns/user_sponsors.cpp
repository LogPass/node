#include "pch.h"
#include "user_sponsors.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions UserSponsorsColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    columnOptions.merge_operator.reset(new AppendMergeOperator);
    return columnOptions;
}

std::vector<UserSponsor> UserSponsorsColumn::getUserSponsors(const UserId& userId, uint32_t page, bool confirmed) const
{
    std::vector<UserSponsor> userSponsor;
    auto s = get<UserId, uint32_t>(userId, boost::endian::endian_reverse(page));
    if (s) {
        ASSERT(s->size() % UserSponsor::SIZE == 0);
        size_t entries = s->size() / UserSponsor::SIZE;
        userSponsor.resize(entries);
        for (auto& entry : userSponsor) {
            (*s)(entry);
        }
    }

    if (confirmed) {
        return userSponsor;
    }

    std::shared_lock lock(m_mutex);
    auto it = m_sponsors.find(userId);
    if (it == m_sponsors.end()) {
        return userSponsor;
    }
    auto pageIt = it->second.find(page);
    if (pageIt == it->second.end()) {
        return userSponsor;
    }

    auto& unconfirmedSponsors = pageIt->second;
    // check if unconfirmed history was already committed
    if (std::find(userSponsor.begin(), userSponsor.end(), unconfirmedSponsors.front()) != userSponsor.end()) {
        return userSponsor;
    }

    userSponsor.insert(userSponsor.end(), unconfirmedSponsors.begin(), unconfirmedSponsors.end());
    return userSponsor;
}

void UserSponsorsColumn::addUserSponsor(const UserId& userId, uint32_t page, const UserSponsor& sponsor)
{
    std::unique_lock lock(m_mutex);
    m_sponsors[userId][page].emplace_back(sponsor);
}

void UserSponsorsColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_sponsors.empty());
    StatefulColumn::load();
}

void UserSponsorsColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [userId, entries] : m_sponsors) {
        for (auto& [page, sponsors] : entries) {
            Serializer key, value;
            key(userId);
            key.put<uint32_t>(boost::endian::endian_reverse(page));
            for (auto& entry : sponsors) {
                value(entry);
            }
            batch.Merge(m_handle, key, value);
        }
    }
}

void UserSponsorsColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_sponsors.clear();
}

void UserSponsorsColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_sponsors.clear();
}

}
}
