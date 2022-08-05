#include "pch.h"
#include "prefix.h"

namespace logpass {

Prefix_ptr Prefix::create(const std::string& prefixId, const UserId& owner, uint32_t blockId)
{
    Prefix_ptr prefix(new Prefix);
    prefix->id = prefixId;
    prefix->owner = owner;
    prefix->created = blockId;
    prefix->committedIn = blockId;
    return prefix;
}

Prefix_cptr Prefix::load(Serializer& s)
{
    Prefix_ptr prefix(new Prefix);
    s(*prefix);
    return prefix;
}

Prefix_ptr Prefix::clone(uint32_t blockId) const
{
    Prefix_ptr newPrefix = std::shared_ptr<Prefix>(new Prefix(*this));
    newPrefix->iteration = iteration + 1;
    newPrefix->committedIn = blockId;
    return newPrefix;
}

void Prefix::serialize(Serializer& s)
{
    s(version);
    if (version != 1) {
        THROW_SERIALIZER_EXCEPTION("Invalid Prefix version");
    }

    s(id);
    s(owner);
    s(iteration);
    s(committedIn);
    s(settings);
    s(entries);
    s(created);
    s(lastEntry);
}

void Prefix::toJSON(json& j) const
{
    j["id"] = getId();
    j["owner"] = owner;
    j["iteration"] = iteration;
    j["committed_in"] = committedIn;
    j["settings"] = settings;
    j["entries"] = entries;
    j["created"] = created;
    j["last_entry"] = lastEntry;
}

bool Prefix::isIdValid(const std::string& prefixId)
{
    if (prefixId.size() < kStoragePrefixMinLength || prefixId.size() > kStoragePrefixMaxLength) {
        return false;
    }
    bool hasInvalidPrefix = std::any_of(prefixId.begin(), prefixId.end(), [](unsigned char c) {
        return !std::isdigit(c) && !std::isupper(c);
    });
    return !hasInvalidPrefix;
}

}
