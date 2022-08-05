#pragma once

namespace logpass {

enum class UserKeyScopes : uint32_t {
    ALL_SCOPES = 0xFFFFFFFFu
};

struct UserKeySettings {
    uint8_t power = 0;
    uint32_t scopes = 0; // reserved

    void serialize(Serializer& s)
    {
        s(power);
        s(scopes);

        if (power == 0 || power > kMaxPower) {
            THROW_SERIALIZER_EXCEPTION("Invalid user key power");
        }
        if (scopes != (uint32_t)(UserKeyScopes::ALL_SCOPES)) {
            THROW_SERIALIZER_EXCEPTION("Invalid user key scopes (currently only all scopes are supported)");
        }
    }

    bool hasScope(UserKeyScopes scope)
    {
        return (scopes & (uint32_t)scope) == (uint32_t)scope;
    }

    bool operator==(const UserKeySettings& second) const = default;

    void toJSON(json& j) const
    {
        j["power"] = power;
        j["scopes"] = scopes;
    }
};

}
