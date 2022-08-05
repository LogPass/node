#pragma once

#include "user_settings.h"
#include "power_level.h"

namespace logpass {

struct UserUpdate;
using UserUpdate_ptr = std::shared_ptr<UserUpdate>;
using UserUpdate_cptr = std::shared_ptr<const UserUpdate>;

struct UserUpdate {
    uint8_t version = 1;

    uint32_t blockId = 0;
    PowerLevel powerLevel;

    UserSettings settings;
    TransactionId transactionId;

    void serialize(Serializer& s)
    {
        s(version);
        if (version != 1) {
            THROW_SERIALIZER_EXCEPTION("Invalid UserUpdate version");
        }

        s(blockId);
        s(powerLevel);

        s(settings);
        s(transactionId);
    }

    void toJSON(json& j) const
    {
        j["block_id"] = blockId;
        j["settings"] = settings;
        j["transaction_id"] = transactionId;
    }
};

}
