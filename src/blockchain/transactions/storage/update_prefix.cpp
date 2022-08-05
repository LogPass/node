#include "pch.h"
#include "update_prefix.h"
#include <database/database.h>

namespace logpass {

Transaction_ptr StorageUpdatePrefixTransaction::create(uint32_t blockId, int16_t pricing, const std::string& prefix,
                                                       const PrefixSettings& settings)
{
    auto transaction = std::make_shared<StorageUpdatePrefixTransaction>();
    transaction->m_blockId = blockId;
    transaction->m_pricing = pricing;
    transaction->m_prefix = prefix;
    transaction->m_settings = settings;
    transaction->reload();
    return transaction;
}

void StorageUpdatePrefixTransaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    Transaction::validate(blockId, database);
    if (!Prefix::isIdValid(m_prefix)) {
        THROW_TRANSACTION_EXCEPTION("Invalid prefix name");
    }

    if (m_settings.allowedUsers.size() > kStoragePrefixMaxAllowedUsers) {
        THROW_TRANSACTION_EXCEPTION("Max "s + std::to_string(kStoragePrefixMaxAllowedUsers) + " users are allowed"s);
    }

    if (m_settings.allowedUsers.count(getUserId()) == 1) {
        THROW_TRANSACTION_EXCEPTION("Owner of prefix is in allowed users by default, it shouldn't be included");
    }

    auto prefix = database.storage.getPrefix(m_prefix);
    if (!prefix) {
        THROW_TRANSACTION_EXCEPTION("Prefix doesn't exist");
    }

    if (prefix->owner != getUserId()) {
        THROW_TRANSACTION_EXCEPTION("Prefix does not belong to transaction user");
    }
}

void StorageUpdatePrefixTransaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    Prefix_ptr prefix = database.storage.getPrefix(m_prefix)->clone(blockId);
    prefix->settings = m_settings;
    database.storage.updatePrefix(prefix);

    Transaction::execute(blockId, database);
}

void StorageUpdatePrefixTransaction::serializeBody(Serializer& s)
{
    s.serialize<uint8_t>(m_prefix);
    s(m_settings);
}

void StorageUpdatePrefixTransaction::toJSON(json& j) const
{
    Transaction::toJSON(j);
    j["prefix"] = m_prefix;
    j["settings"] = m_settings;
}

}
