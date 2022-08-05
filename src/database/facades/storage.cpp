#include "pch.h"
#include "storage.h"

namespace logpass {
namespace database {

Prefix_cptr StorageFacade::getPrefix(const std::string& prefixId) const
{
    return m_prefixes->getPrefix(prefixId, m_confirmed);
}

void StorageFacade::addPrefix(const Prefix_cptr& prefix)
{
    m_prefixes->addPrefix(prefix);
}

void StorageFacade::updatePrefix(const Prefix_cptr& prefix)
{
    m_prefixes->updatePrefix(prefix);
}

uint64_t StorageFacade::getPrefixesCount() const
{
    return m_prefixes->getPrefixesCount(m_confirmed);
}

StorageEntry_cptr StorageFacade::getEntry(const std::string& prefix, const std::string& key) const
{
    return m_entries->getEntry(prefix, key, m_confirmed);
}

void StorageFacade::addEntry(const std::string& prefixId, const std::string& key, const StorageEntry_cptr& entry)
{
    m_entries->addEntry(prefixId, key, entry);
}

uint64_t StorageFacade::getEntriesCount() const
{
    return m_entries->getEntriesCount(m_confirmed);
}

std::vector<TransactionId> StorageFacade::getTransasctionsForPrefix(const std::string& prefix, uint32_t page) const
{
    return m_entries->getTransasctionsForPrefix(prefix, page, m_confirmed);
}

}
}
