#pragma once

#include "stateful_column.h"

namespace logpass {
namespace database {

struct DefaultColumnState : public ColumnState {
    uint16_t blockchainVersion = 1;
    int16_t pricing = 1;

    void serialize(Serializer& s)
    {
        ColumnState::serialize(s);
        s(blockchainVersion);
        s(pricing);
    }
};

// default database column, required by rocksdb
class DefaultColumn : public StatefulColumn<DefaultColumnState> {
public:
    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return rocksdb::kDefaultColumnFamilyName; // "default"
    }

    static rocksdb::ColumnFamilyOptions getOptions();


    void setVersion(uint16_t version);
    uint16_t getVersion(bool confirmed) const;

    void setPricing(int16_t pricing);
    int16_t getPricing(bool confirmed) const;

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

};

}
}
