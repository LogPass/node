#pragma once

#include "user_key_settings.h"

namespace logpass {

struct UserSupervisorSettings {
    uint8_t power = 0;
    uint32_t scopes = 0; // reserved for future use

    void serialize(Serializer& s)
    {
        s(power);
        s(scopes);

        if (power == 0 || power > kMaxPower) {
            THROW_SERIALIZER_EXCEPTION("Invalid supervisor power");
        }
        if (scopes != (uint32_t)(UserKeyScopes::ALL_SCOPES)) {
            THROW_SERIALIZER_EXCEPTION("Invalid supervisor scopes (currently only all scopes are supported)");
        }
    }

    bool hasScope(UserKeyScopes scope)
    {
        return (scopes & (uint32_t)scope) == (uint32_t)scope;
    }

    bool operator==(const UserSupervisorSettings& second) const = default;

    void toJSON(json& j) const
    {
        j["power"] = power;
        j["scopes"] = scopes;
    }
};

}
