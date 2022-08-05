#pragma once

#include "user_key_settings.h"
#include "user_settings_validation_error.h"
#include "invalid_power_exception.h"

namespace logpass {

class UserKeys {
public:
    static UserKeys create(const PublicKey& key, uint8_t power = 1, UserKeyScopes scopes = UserKeyScopes::ALL_SCOPES)
    {
        UserKeys userKeys;
        userKeys.m_keys.emplace(key, UserKeySettings{ power, (uint32_t)scopes });
        return userKeys;
    }

    static UserKeys create(const std::map<PublicKey, uint8_t>& keys)
    {
        UserKeys userKeys;
        for (auto& [key, power] : keys) {
            userKeys.m_keys.emplace(key, UserKeySettings{ power, (uint32_t)UserKeyScopes::ALL_SCOPES });
        }
        return userKeys;
    }

    void validate() const
    {
        if (m_keys.empty()) {
            THROW_USER_SETTINGS_EXCEPTION("Empty user keys");
        }
        if (m_keys.size() > kUserMaxKeys) {
            THROW_USER_SETTINGS_EXCEPTION("Too many user keys");
        }
    }

    bool hasKey(const PublicKey& key) const
    {
        return m_keys.contains(key);
    }

    size_t size() const
    {
        return m_keys.size();
    }

    std::set<PublicKey> getPublicKeys() const
    {
        std::set<PublicKey> keys;
        for (auto& [key, _] : m_keys) {
            keys.insert(key);
        }
        return keys;
    }

    void serialize(Serializer& s)
    {
        s.serialize<uint8_t>(m_keys);
    }

    bool operator==(const UserKeys& second) const = default;

    void toJSON(json& j) const
    {
        for (auto& [key, value] : m_keys) {
            j.emplace(key.toString(), value);
        }
    }

    std::map<PublicKey, UserKeySettings>::const_iterator begin() const
    {
        return m_keys.begin();
    }

    std::map<PublicKey, UserKeySettings>::const_iterator end() const
    {
        return m_keys.end();
    }

private:
    std::map<PublicKey, UserKeySettings> m_keys;
};

//inline void to_json(json& j, const UserKeySettings& s)
//{
//    s.toJSON(j);
//}

}
