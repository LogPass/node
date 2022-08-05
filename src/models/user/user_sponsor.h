#pragma once

namespace logpass {

struct UserSponsor {
    // size in bytes
    static constexpr size_t SIZE = 4 + 1 + Hash::SIZE;

    uint32_t blockId;
    Hash sponsor;
    uint8_t transactions;

    UserSponsor() = default;
    UserSponsor(uint32_t blockId, const Hash& sponsor, uint8_t transactions) :
        blockId(blockId), sponsor(sponsor), transactions(transactions)
    {};

    void serialize(Serializer& s)
    {
        s(blockId);
        s(sponsor);
        s(transactions);
    }

    void toJSON(json& j) const
    {
        j = {
            { "block_id", blockId },
            { "sponsor", sponsor },
            { "transactions", transactions }
        };
    }

    auto operator<=>(const UserSponsor&) const = default;
};

}
