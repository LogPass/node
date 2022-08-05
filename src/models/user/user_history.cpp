#include "pch.h"
#include "user_history.h"

namespace logpass {

void UserHistory::serialize(Serializer& s)
{
    s(blockId);
    if (s.reader()) {
        type = (UserHistoryType)s.get<uint8_t>();
    } else {
        s.put<uint8_t>((uint8_t)type);
    }
    s(transactionId);
}

void UserHistory::toJSON(json& j) const
{
    std::string strType = "";
    switch (type) {
    case UserHistoryType::OUTGOING_TRANSACTION:
        strType = "outgoing_transaction";
        break;
    case UserHistoryType::INCOMING_TRANSACTION:
        strType = "incoming_transaction";
        break;
    case UserHistoryType::SPONSORED_TRANSACTION:
        strType = "sponsored_transaction";
        break;
    case UserHistoryType::UPDATE:
        strType = "update";
        break;
    }

    j = {
        { "block_id", blockId },
        { "type", strType },
        { "transaction_id", transactionId }
    };
}

}
