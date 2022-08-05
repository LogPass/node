#include "pch.h"
#include "users.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions UsersColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    return columnOptions;
}

User_cptr UsersColumn::getUser(const UserId& userId, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_users.find(userId);
        if (it != m_users.end()) {
            return it->second;
        }
    }

    Serializer_ptr s = get<UserId>(userId);
    if (!s) {
        return nullptr;
    }
    return User::load(*s, state(confirmed).blockId);
}

User_cptr UsersColumn::getRandomUser(bool confirmed) const
{
    UserId randomUserId(PublicKey::generateRandom());

    rocksdb::Iterator* it = m_db->NewIterator(rocksdb::ReadOptions(), m_handle);
    it->Seek(randomUserId);
    if (!it->Valid()) {
        it->SeekForPrev(randomUserId);
    }
    if (!it->Valid() || it->key().empty()) {
        delete it;
        if (!confirmed) {
            std::shared_lock lock(m_mutex);
            if (!m_users.empty()) {
                return m_users.begin()->second;
            }
        }
        return nullptr;
    }

    Serializer s(it->value());    
    auto user = User::load(s, state(confirmed).blockId);
    delete it;
    return user;
}

void UsersColumn::addUser(const User_cptr& user)
{
    ASSERT(user->getId().isValid());
    ASSERT(!getUser(user->getId(), false));
    ASSERT(user->committedIn > 0 && user->iteration == 0);
    std::unique_lock lock(m_mutex);
    m_users[user->getId()] = user;
    state().users += 1;
    state().tokens += user->tokens;
}

void UsersColumn::updateUser(const User_cptr& user)
{
    auto oldUser = getUser(user->getId(), false);
    ASSERT(user->committedIn >= oldUser->committedIn && user->iteration == oldUser->iteration + 1);
    std::unique_lock lock(m_mutex);
    m_users[user->getId()] = user;
    state().tokens -= oldUser->tokens;
    state().tokens += user->tokens;
}

void UsersColumn::preloadUser(const UserId& userId)
{
    std::unique_lock lock(m_mutex);
    m_usersToPreload.insert(userId);
}

uint64_t UsersColumn::getUsersCount(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).users;
}

uint64_t UsersColumn::getTokens(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).tokens;
}

void UsersColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_users.empty());
    StatefulColumn::load();
}

void UsersColumn::preload(uint32_t blockId)
{
    std::set<UserId> usersToPreload;
    {
        std::unique_lock lock(m_mutex);
        usersToPreload = std::move(m_usersToPreload);
        m_usersToPreload.clear();
    }

    std::vector<UserId> missingUserIds;
    for (auto& userId : usersToPreload) {
        if (!m_users.contains(userId)) {
            missingUserIds.emplace_back(userId);
        }
    }

    if (missingUserIds.empty()) {
        return;
    }

    std::map<UserId, User_cptr> missingUsers;
    auto results = multiGet(missingUserIds);
    for (size_t i = 0; i < results.size(); ++i) {
        if (!results[i]) {
            missingUsers.emplace(missingUserIds[i], nullptr);
        } else {
            missingUsers.emplace(missingUserIds[i], User::load(*results[i], blockId));
        }
    }

    std::unique_lock lock(m_mutex);
    m_users.insert(missingUsers.begin(), missingUsers.end());
}

void UsersColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    StatefulColumn::prepare(blockId, batch);

    for (auto& [userId, user] : m_users) {
        if (!user || user->committedIn != blockId) {
            continue; // cached, not updated user
        }
        Serializer s;
        s(user);
        batch.Put(m_handle, userId, s);
    }
}

void UsersColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_users.clear();
    m_usersToPreload.clear();
}

void UsersColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_users.clear();
    m_usersToPreload.clear();
}

}
}
