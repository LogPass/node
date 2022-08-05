#pragma once

namespace logpass {

struct UserHistory;
using UserHistory_ptr = std::shared_ptr<UserHistory>;
using UserHistory_cptr = std::shared_ptr<const UserHistory>;

enum class UserHistoryType : uint8_t {
    OUTGOING_TRANSACTION = 0,
    INCOMING_TRANSACTION = 1,
    SPONSORED_TRANSACTION = 2,
    UPDATE = 3,
};

struct UserHistory {
    // size in bytes
    static constexpr size_t SIZE = 4 + 1 + TransactionId::SIZE;

    uint32_t blockId;
    UserHistoryType type;
    TransactionId transactionId;

    UserHistory() = default;
    UserHistory(uint32_t blockId, UserHistoryType type, const TransactionId& transactionId) :
        blockId(blockId), type(type), transactionId(transactionId)
    {};

    void serialize(Serializer& s);

    void toJSON(json& j) const;

    auto operator<=>(const UserHistory& h) const = default;
};

}
