#pragma once

#include "user_settings_validation_error.h"
#include "user_supervisor_settings.h"
#include "invalid_power_exception.h"

namespace logpass {

class UserSupervisors {
public:
    static UserSupervisors create(const UserId& supervisorUserId, uint8_t power,
                                  UserKeyScopes scopes = UserKeyScopes::ALL_SCOPES)
    {
        UserSupervisors userSupervisors;
        userSupervisors.m_supervisors.emplace(supervisorUserId, UserSupervisorSettings{ power, (uint32_t)scopes });
        return userSupervisors;
    }

    static UserSupervisors create(const std::map<UserId, uint8_t>& supervisors)
    {
        UserSupervisors userSupervisors;
        for (auto& [userId, power] : supervisors) {
            userSupervisors.m_supervisors.emplace(userId,
                                                  UserSupervisorSettings{ power, (uint32_t)UserKeyScopes::ALL_SCOPES });
        }
        return userSupervisors;
    }

    static UserSupervisors create(const std::map<UserId, std::pair<uint8_t, UserKeyScopes>>& supervisors)
    {
        UserSupervisors userSupervisors;
        for (auto& [userId, settings] : supervisors) {
            userSupervisors.m_supervisors.emplace(userId,
                                                  UserSupervisorSettings{ settings.first, (uint32_t)settings.second });
        }
        return userSupervisors;
    }

    void validate() const
    {
        if (m_supervisors.size() > kUserMaxSupervisors) {
            THROW_USER_SETTINGS_EXCEPTION("Too many supervisors");
        }
    }

    bool hasSupervisor(const UserId& userId) const
    {
        return m_supervisors.contains(userId);
    }

    UserSupervisorSettings getSupervisorSettings(const UserId& userId) const
    {
        auto it = m_supervisors.find(userId);
        ASSERT(it != m_supervisors.end());
        return it->second;
    }

    size_t size() const
    {
        return m_supervisors.size();
    }

    void serialize(Serializer& s)
    {
        s.serialize<uint8_t>(m_supervisors);
    }

    bool operator==(const UserSupervisors& second) const = default;

    void toJSON(json& j) const
    {
        for (auto& [userId, value] : m_supervisors) {
            j.emplace(userId.toString(), value);
        }
    }

    std::map<UserId, UserSupervisorSettings>::const_iterator begin() const
    {
        return m_supervisors.begin();
    }

    std::map<UserId, UserSupervisorSettings>::const_iterator end() const
    {
        return m_supervisors.end();
    }

private:
    std::map<UserId, UserSupervisorSettings> m_supervisors;
};

}
