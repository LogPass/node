#include "pch.h"
#include "storage_prefixes.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions StoragePrefixesColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    return columnOptions;
}

Prefix_cptr StoragePrefixesColumn::getPrefix(const std::string& prefixId, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_prefixes.find(prefixId);
        if (it != m_prefixes.end())
            return it->second;
    }

    Serializer sKey;
    sKey.serialize<uint8_t>(prefixId);
    Serializer_ptr s = get(sKey);
    if (!s)
        return nullptr;
    return Prefix::load(*s);
}

void StoragePrefixesColumn::addPrefix(const Prefix_cptr& prefix)
{
    ASSERT(!getPrefix(prefix->getId(), false));
    std::unique_lock lock(m_mutex);
    m_prefixes[prefix->getId()] = prefix;
    state().prefixes += 1;
}

void StoragePrefixesColumn::updatePrefix(const Prefix_cptr& prefix)
{
    ASSERT(getPrefix(prefix->getId(), false));
    std::unique_lock lock(m_mutex);
    m_prefixes[prefix->getId()] = prefix;
}

uint64_t StoragePrefixesColumn::getPrefixesCount(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).prefixes;
}

void StoragePrefixesColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_prefixes.empty());
    StatefulColumn::load();
}

void StoragePrefixesColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [prefixId, prefix] : m_prefixes) {
        Serializer sKey, sVal;
        sKey.serialize<uint8_t>(prefixId);
        sVal(prefix);
        batch.Put(m_handle, sKey, sVal);
    }
}

void StoragePrefixesColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_prefixes.clear();
}

void StoragePrefixesColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_prefixes.clear();
}

}
}
