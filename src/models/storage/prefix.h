#pragma once

#include "prefix_settings.h"

namespace logpass {

struct Prefix;
using Prefix_ptr = std::shared_ptr<Prefix>;
using Prefix_cptr = std::shared_ptr<const Prefix>;

struct Prefix {
    uint8_t version = 1;
    std::string id;
    UserId owner;
    uint64_t iteration = 0;
    uint32_t committedIn = 0;

    PrefixSettings settings;

    uint64_t entries = 0;
    uint32_t created = 0;
    uint32_t lastEntry = 0;

protected:
    Prefix() = default;
    Prefix(const Prefix&) = default;
    Prefix& operator = (const Prefix&) = delete;

public:
    static Prefix_ptr create(const std::string& prefixId, const UserId& owner, uint32_t blockId);
    static Prefix_cptr load(Serializer& s);

    Prefix_ptr clone(uint32_t blockId) const;
    void serialize(Serializer& s);

    // for containers
    std::string getId() const
    {
        return id;
    }

    void toJSON(json& j) const;

    static bool isIdValid(const std::string& prefixId);
};

}
