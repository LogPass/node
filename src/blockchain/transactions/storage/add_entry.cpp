#include "pch.h"
#include "add_entry.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr StorageAddEntryTransaction::create(uint32_t blockId, int16_t pricing, const std::string& prefix,
                                                   const std::string& key, const std::string& value)
{
    auto transaction = std::make_shared<StorageAddEntryTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_prefix = prefix;
    transaction->m_key = key;
    transaction->m_value = value;
    transaction->reload();
    return transaction;
}

void StorageAddEntryTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);
    if (!Prefix::isIdValid(m_prefix)) {
        THROW_TRANSACTION_EXCEPTION("Invalid prefix");
    }

    if (m_key.empty()) {
        THROW_TRANSACTION_EXCEPTION("Key can't be empty");
    }

    if (m_value.size() > kStorageEntryMaxValueLength) {
        THROW_TRANSACTION_EXCEPTION("Value is too long");
    }

    auto prefix = database.storage.getPrefix(m_prefix);
    if (!prefix) {
        THROW_TRANSACTION_EXCEPTION("Prefix doesn't exist");
    }

    if (prefix->owner != getUserId() && !prefix->settings.allowedUsers.contains(getUserId())) {
        THROW_TRANSACTION_EXCEPTION("User is not allowed to use selected prefix");
    }

    if (database.storage.getEntry(m_prefix, m_key)) {
        THROW_TRANSACTION_EXCEPTION("Storage with given prefix and key already exists");
    }
}

void StorageAddEntryTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    auto prefix = database.storage.getPrefix(m_prefix)->clone(blockId);
    uint32_t entryId = prefix->entries;
    prefix->entries += 1;
    prefix->lastEntry = blockId;
    database.storage.updatePrefix(prefix);

    auto entry = std::make_shared<StorageEntry>();
    entry->id = entryId;
    entry->transactionId = getId();

    database.storage.addEntry(m_prefix, m_key, entry);

    Transaction::execute(blockId, database);
}

void StorageAddEntryTransaction::serializeBody(Serializer& s)
{
    s.serialize<uint8_t>(m_prefix);
    s.serialize<uint8_t>(m_key);
    s.serialize<uint16_t>(m_value);
}

uint64_t StorageAddEntryTransaction::getFee() const noexcept
{
    return Transaction::getFee() * (1 + (m_key.size() + m_value.size()) / 1024);
}

void StorageAddEntryTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["prefix"] = m_prefix;
    j["key"] = base64url::encode(m_key);
    j["value"] = base64url::encode(m_value);
}

}
