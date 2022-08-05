#pragma once

#include "user_keys.h"
#include "user_security_rules.h"
#include "user_settings_validation_error.h"
#include "user_supervisors.h"

namespace logpass {

struct UserSettings {
    uint8_t version = 1;
    UserKeys keys;
    UserSupervisors supervisors;
    UserSecurityRules rules;

    void serialize(Serializer& s)
    {
        s(version);
        if (version != 1) {
            THROW_SERIALIZER_EXCEPTION("Invalid UserSettings version");
        }

        s(keys);
        s(supervisors);
        s(rules);
    }

    void validate() const
    {
        keys.validate();
        supervisors.validate();
        rules.validate();
    }

    bool operator==(const UserSettings& second) const = default;

    void toJSON(json& j) const
    {
        j["version"] = version;
        j["keys"] = keys;
        j["supervisors"] = supervisors;
        j["rules"] = rules;
    }
};

}
