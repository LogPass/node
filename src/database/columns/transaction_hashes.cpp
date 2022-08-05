#include "pch.h"
#include "transaction_hashes.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions TransactionHashesColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    rocksdb::BlockBasedTableOptions table_options;
    table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));
    columnOptions.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
    return columnOptions;
}

bool TransactionHashesColumn::hasTransactionHash(uint32_t transactionBlockId, const Hash& hash,
                                                bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_hashes.find(transactionBlockId);
        if (it != m_hashes.end()) {
            if (it->second.contains(hash)) {
                return true;
            }
        }
    }

    auto s = get(boost::endian::endian_reverse(transactionBlockId), hash);
    return s != nullptr;
}

void TransactionHashesColumn::addTransactionHashHash(uint32_t transactionBlockId, const Hash& hash)
{
    ASSERT(!hasTransactionHash(transactionBlockId, hash, false));
    std::unique_lock lock(m_mutex);
    m_hashes[transactionBlockId].insert(hash);
}

void TransactionHashesColumn::load()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::load();
}

void TransactionHashesColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [blockIdLE, hashes] : m_hashes) {
        uint32_t blockId = boost::endian::endian_reverse(blockIdLE);
        for (auto& hash : hashes) {
            put<uint32_t, Hash>(batch, blockId, hash, rocksdb::Slice());
        }
    }

    if (blockId > kTransactionMaxBlockIdDifference) {
        Serializer last_key;
        last_key.put<uint32_t>(boost::endian::endian_reverse(blockId - kTransactionMaxBlockIdDifference));
        batch.DeleteRange(m_handle, rocksdb::Slice("\0"s), last_key); // "\0" to skip state which is ""
    }
}

void TransactionHashesColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_hashes.clear();
}

void TransactionHashesColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_hashes.clear();
}

}
}
