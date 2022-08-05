#pragma once

namespace logpass {

struct PrefixSettings {
    uint8_t version = 1;
    std::set<UserId> allowedUsers;

    void serialize(Serializer& s)
    {
        s(version);
        if (version != 1) {
            THROW_SERIALIZER_EXCEPTION("Invalid UserSettings version");
        }

        s.serialize<uint8_t>(allowedUsers);
    }

    void toJSON(json& j) const
    {
        j["version"] = version;
        j["allowed_users"] = allowedUsers;
    }
};

}
