#pragma once

#include "stateful_column.h"
#include <blockchain/block/block.h>

namespace logpass {
namespace database {

struct UserUpdatesColumnState : public ColumnState {

};

// Keeps list of updated users in given block
class UserUpdatesColumn : public StatefulColumn<UserUpdatesColumnState> {
public:
    constexpr static size_t PAGE_SIZE = 100;

    using StatefulColumn::StatefulColumn;

    // returns name of the column
    static std::string getName()
    {
        return "user_updates";
    }

    // returns options for column
    static rocksdb::ColumnFamilyOptions getOptions();

    //
    void addUpdatedUserId(uint32_t blockId, const UserId& userId);
    //
    uint32_t getUpdatedUserIdsCount(uint32_t blockId, bool confirmed) const;
    //
    std::set<UserId> getUpdatedUserIds(uint32_t blockId, uint32_t page, bool confirmed) const;

    // loads state
    void load() override;
    // 
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    //
    void commit() override;
    // removes temporary changes
    void clear() override;

private:
    std::map<uint32_t, std::set<UserId>> m_userIdsPerBlock;
};

}
}
