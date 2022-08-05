#include "pch.h"
#include "default.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions DefaultColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    return columnOptions;
}

void DefaultColumn::setVersion(uint16_t version)
{
    std::unique_lock lock(m_mutex);
    state().blockchainVersion = version;
}

uint16_t DefaultColumn::getVersion(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).blockchainVersion;
}

void DefaultColumn::setPricing(int16_t pricing)
{
    ASSERT(pricing > 0);
    std::unique_lock lock(m_mutex);
    state().pricing = pricing;
}

int16_t DefaultColumn::getPricing(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).pricing;
}

void DefaultColumn::load()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::load();
}

void DefaultColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);
}

void DefaultColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
}

void DefaultColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
}

}
}
