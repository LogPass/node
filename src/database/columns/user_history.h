#pragma once

#include "stateful_column.h"
#include <models/user/user_history.h>

namespace logpass {
namespace database {

struct UserHistoryColumnState : public ColumnState {

};

// keeps history of user transactions
class UserHistoryColumn : public StatefulColumn<UserHistoryColumnState> {
public:
    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return "user_history";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    std::vector<UserHistory> getUserHistory(const UserId& userId, uint32_t page, bool confirmed) const;

    void addUserHistory(const UserId& userId, uint32_t page, const UserHistory& history);

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    std::map<UserId, std::map<uint32_t, std::vector<UserHistory>>> m_transactions;
};

}
}
