#include "pch.h"
#include "create_prefix.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr StorageCreatePrefixTransaction::create(uint32_t blockId, int16_t pricing, const std::string& prefix)
{
    auto transaction = std::make_shared<StorageCreatePrefixTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_prefix = prefix;
    transaction->reload();
    return transaction;
}

void StorageCreatePrefixTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);
    if (!Prefix::isIdValid(m_prefix)) {
        THROW_TRANSACTION_EXCEPTION("Invalid prefix name");
    }
    if (database.storage.getPrefix(m_prefix)) {
        THROW_TRANSACTION_EXCEPTION("Prefix already exist");
    }
}

void StorageCreatePrefixTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    Prefix_ptr prefix = Prefix::create(m_prefix, getUserId(), blockId);
    database.storage.addPrefix(prefix);
    Transaction::execute(blockId, database);
}

void StorageCreatePrefixTransaction::serializeBody(Serializer& s)
{
    s.serialize<uint8_t>(m_prefix);
}

void StorageCreatePrefixTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["prefix"] = m_prefix;
}

}
