#include "pch.h"
#include "storage_entries.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions StorageEntriesColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    columnOptions.merge_operator.reset(new AppendMergeOperator);
    return columnOptions;
}

StorageEntry_cptr StorageEntriesColumn::getEntry(const std::string& prefix, const std::string& key,
                                                 bool confirmed) const
{
    if (prefix.empty() || key.empty())
        return nullptr;

    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_entries.find(prefix);
        if (it != m_entries.end()) {
            auto it2 = it->second.find(key);
            if (it2 != it->second.end())
                return it2->second;
        }
    }

    Serializer sKey;
    sKey.serialize<uint8_t>(prefix);
    sKey.serialize<uint8_t>(key);

    Serializer_ptr s = get(sKey);
    if (!s)
        return nullptr;
    auto entry = std::make_shared<StorageEntry>();
    (*s)(entry);
    return entry;
}

void StorageEntriesColumn::addEntry(const std::string& prefix, const std::string& key, const StorageEntry_cptr& entry)
{
    ASSERT(!prefix.empty() && !key.empty());
    std::unique_lock lock(m_mutex);
    m_entries[prefix][key] = entry;
    m_prefixHistory[prefix][entry->id / PAGE_SIZE].push_back(entry->transactionId);
    state().entries += 1;
}

uint64_t StorageEntriesColumn::getEntriesCount(bool confirmed) const
{
    std::unique_lock lock(m_mutex);
    return state(confirmed).entries;
}

std::vector<TransactionId> StorageEntriesColumn::getTransasctionsForPrefix(const std::string& prefix, uint32_t page,
                                                                           bool confirmed) const
{
    if (prefix.empty())
        return {};

    std::vector<TransactionId> prefixHistory;
    Serializer sKey;
    sKey.serialize<uint8_t>(prefix);
    sKey.put<uint8_t>(0x00);
    sKey.put<uint32_t>(boost::endian::endian_reverse(page));
    auto s = get(sKey);
    if (s) {
        ASSERT(s->size() % TransactionId::SIZE == 0);
        size_t entries = s->size() / TransactionId::SIZE;
        prefixHistory.resize(entries);
        for (auto& entry : prefixHistory) {
            (*s)(entry);
        }
    }

    if (confirmed)
        return prefixHistory;

    std::shared_lock lock(m_mutex);
    auto it = m_prefixHistory.find(prefix);
    if (it == m_prefixHistory.end())
        return prefixHistory;
    auto pageIt = it->second.find(page);
    if (pageIt == it->second.end())
        return prefixHistory;
    auto& unconfirmedHistory = pageIt->second;

    // check if unconfirmed history was already committed
    if (std::find(prefixHistory.begin(), prefixHistory.end(), unconfirmedHistory.front()) != prefixHistory.end())
        return prefixHistory;

    prefixHistory.insert(prefixHistory.end(), unconfirmedHistory.begin(), unconfirmedHistory.end());
    return prefixHistory;
}

void StorageEntriesColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_entries.empty() && m_prefixHistory.empty());
    StatefulColumn::load();
}

void StorageEntriesColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [prefix, entries] : m_entries) {
        for (auto& [key, entry] : entries) {
            Serializer sKey, sValue;
            sKey.serialize<uint8_t>(prefix);
            sKey.serialize<uint8_t>(key);
            sValue(entry);
            put(batch, sKey, sValue);
        }
    }

    for (auto& [prefix, pages] : m_prefixHistory) {
        for (auto& [page, transactionIds] : pages) {
            Serializer sKey, sValue;
            sKey.serialize<uint8_t>(prefix);
            sKey.put<uint8_t>(0);
            sKey.put<uint32_t>(boost::endian::endian_reverse(page));
            for (auto& transactionId : transactionIds) {
                sValue(transactionId);
            }
            batch.Merge(m_handle, sKey, sValue);
        }
    }
}

void StorageEntriesColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_entries.clear();
    m_prefixHistory.clear();
}

void StorageEntriesColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_entries.clear();
    m_prefixHistory.clear();
}

}
}
