#pragma once

#include "stateful_column.h"
#include <models/user/user.h>

namespace logpass {
namespace database {

struct UsersColumnState : public ColumnState {
    uint64_t users = 0;
    uint64_t tokens = 0;

    void serialize(Serializer& s)
    {
        ColumnState::serialize(s);
        s(users);
        s(tokens);
    }
};

// keeps users
class UsersColumn : public StatefulColumn<UsersColumnState> {
public:
    using StatefulColumn::StatefulColumn;

    static std::string getName()
    {
        return "users";
    }

    static rocksdb::ColumnFamilyOptions getOptions();

    User_cptr getUser(const UserId& userId, bool confirmed) const;
    User_cptr getRandomUser(bool confirmed) const;
    void addUser(const User_cptr& user);
    void updateUser(const User_cptr& user);
    void preloadUser(const UserId& userId);
    uint64_t getUsersCount(bool confirmed) const;
    uint64_t getTokens(bool confirmed) const;

    void load() override;
    void preload(uint32_t blockId) override;
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    void commit() override;
    void clear() override;

private:
    // changed users
    std::map<UserId, User_cptr> m_users;
    // users to preload
    std::set<UserId> m_usersToPreload;
};

}
}
