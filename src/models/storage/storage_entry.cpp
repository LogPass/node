#include "pch.h"

#include "storage_entry.h"

namespace logpass {

void StorageEntry::serialize(Serializer& s)
{
    s(id);
    s(transactionId);
}

void StorageEntry::toJSON(json& j) const
{
    j["id"] = id;
    j["transaction_id"] = transactionId;
}

}
