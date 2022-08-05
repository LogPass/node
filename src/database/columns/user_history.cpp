#include "pch.h"
#include "user_history.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions UserHistoryColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    columnOptions.merge_operator.reset(new AppendMergeOperator);
    return columnOptions;
}

std::vector<UserHistory> UserHistoryColumn::getUserHistory(const UserId& userId, uint32_t page,
                                                           bool confirmed) const
{
    std::vector<UserHistory> userHistory;
    auto s = get<UserId, uint32_t>(userId, boost::endian::endian_reverse(page));
    if (s) {
        ASSERT(s->size() % UserHistory::SIZE == 0);
        size_t entries = s->size() / UserHistory::SIZE;
        userHistory.resize(entries);
        for (auto& entry : userHistory) {
            (*s)(entry);
        }
    }

    if (confirmed) {
        return userHistory;
    }

    std::shared_lock lock(m_mutex);
    auto it = m_transactions.find(userId);
    if (it == m_transactions.end()) {
        return userHistory;
    }
    auto pageIt = it->second.find(page);
    if (pageIt == it->second.end()) {
        return userHistory;
    }
    auto& unconfirmedUserHistory = pageIt->second;
    // check if unconfirmed history was already committed
    if (std::find(userHistory.begin(), userHistory.end(), unconfirmedUserHistory.front()) != userHistory.end()) {
        return userHistory;
    }

    userHistory.insert(userHistory.end(), unconfirmedUserHistory.begin(), unconfirmedUserHistory.end());
    return userHistory;
}

void UserHistoryColumn::addUserHistory(const UserId& userId, uint32_t page, const UserHistory& history)
{
    std::unique_lock lock(m_mutex);
    m_transactions[userId][page].emplace_back(history);
}

void UserHistoryColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_transactions.empty());
    StatefulColumn::load();
}

void UserHistoryColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [userId, transactions] : m_transactions) {
        for (auto& [page, history] : transactions) {
            Serializer key, value;
            key(userId);
            key.put<uint32_t>(boost::endian::endian_reverse(page));
            for (auto& entry : history) {
                value(entry);
            }
            batch.Merge(m_handle, key, value);
        }
    }
}

void UserHistoryColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_transactions.clear();
}

void UserHistoryColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_transactions.clear();
}

}
}
