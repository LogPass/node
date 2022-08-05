#pragma once

#include "stateful_column.h"
#include <models/user/user.h>
#include <models/user/user_sponsor.h>

namespace logpass {
namespace database {

struct UserSponsorsColumnState : public ColumnState {

};

class UserSponsorsColumn : public StatefulColumn<UserSponsorsColumnState> {
public:
    using StatefulColumn::StatefulColumn;

    constexpr static size_t PAGE_SIZE = 100;

    static std::string getName()
    {
        return "user_sponsors";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    void addUserSponsor(const UserId& userId, uint32_t page, const UserSponsor& sponsor);
    std::vector<UserSponsor> getUserSponsors(const UserId& userId, uint32_t page, bool confirmed) const;

    void load() override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    std::map<UserId, std::map<uint32_t, std::vector<UserSponsor>>> m_sponsors;
};

}
}
