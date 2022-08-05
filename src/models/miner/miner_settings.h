#pragma once

namespace logpass {

struct MinerSettings {
    uint8_t version = 1;
    Endpoint endpoint;
    std::string api;
    std::string name;
    std::string website;
    std::string description;

    void serialize(Serializer& s)
    {
        s(version);
        if (version != 1) {
            THROW_SERIALIZER_EXCEPTION("Invalid MinerSettings version");
        }

        s(endpoint);
        s.serialize<uint8_t>(api);
        s.serialize<uint8_t>(name);
        s.serialize<uint8_t>(website);
        s.serialize<uint8_t>(description);

        if (api.size() > 64) {
            THROW_SERIALIZER_EXCEPTION("Too long name");
        }
        if (name.size() > 64) {
            THROW_SERIALIZER_EXCEPTION("Too long name");
        }
        if (website.size() > 64) {
            THROW_SERIALIZER_EXCEPTION("Too long website");
        }
    }

    void toJSON(json& j) const
    {
        j["version"] = version;
        j["endpoint"] = endpoint;
        j["api"] = api;
        j["name"] = name;
        j["description"] = description;
        j["website"] = website;
    }
};

}
