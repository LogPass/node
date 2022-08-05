#pragma once

namespace logpass {

struct StorageEntry;

using StorageEntry_ptr = std::shared_ptr<StorageEntry>;
using StorageEntry_cptr = std::shared_ptr<const StorageEntry>;

struct StorageEntry {
    uint32_t id = 0;
    TransactionId transactionId;

    void serialize(Serializer& s);

    void toJSON(json& j) const;
};

}
