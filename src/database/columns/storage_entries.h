#pragma once

#include "stateful_column.h"
#include <models/storage/storage_entry.h>

namespace logpass {
namespace database {

struct StorageEntriesColumnState : public ColumnState {
    uint64_t entries = 0;

    void serialize(Serializer& s)
    {
        ColumnState::serialize(s);
        s(entries);
    }
};

// keeps storage <prefix, key> and <prefix, index> pair poiniting to transactionId
class StorageEntriesColumn : public StatefulColumn<StorageEntriesColumnState> {
public:
    constexpr static size_t PAGE_SIZE = 100;

    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return "storage_entries";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    StorageEntry_cptr getEntry(const std::string& prefix, const std::string& key, bool confirmed) const;

    void addEntry(const std::string& prefix, const std::string& key, const StorageEntry_cptr& entry);

    uint64_t getEntriesCount(bool confirmed) const;

    std::vector<TransactionId> getTransasctionsForPrefix(const std::string& prefix, uint32_t page,
                                                         bool confirmed) const;

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    std::map<std::string, std::map<std::string, StorageEntry_cptr>> m_entries;
    std::map<std::string, std::map<uint32_t, std::vector<TransactionId>>> m_prefixHistory;
};

}
}
