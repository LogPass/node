#pragma once

#include "stateful_column.h"
#include <models/storage/prefix.h>

namespace logpass {
namespace database {

struct StoragePrefixesColumnState : public ColumnState {
    uint64_t prefixes = 0;

    void serialize(Serializer& s)
    {
        ColumnState::serialize(s);
        s(prefixes);
    }
};

// keeps storage prefixes
class StoragePrefixesColumn : public StatefulColumn<StoragePrefixesColumnState> {
public:
    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return "storage_prefixes";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    Prefix_cptr getPrefix(const std::string& prefixId, bool confirmed) const;

    void addPrefix(const Prefix_cptr& prefix);

    void updatePrefix(const Prefix_cptr& prefix);

    uint64_t getPrefixesCount(bool confirmed) const;

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    std::map<std::string, Prefix_cptr> m_prefixes;
};

}
}
