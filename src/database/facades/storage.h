#pragma once

#include "facade.h"

#include <database/columns/storage_entries.h>
#include <database/columns/storage_prefixes.h>
#include <models/storage/storage_entry.h>
#include <models/storage/prefix.h>

namespace logpass {
namespace database {

class StorageFacade : public Facade {
public:
    StorageFacade(StorageEntriesColumn* entries, StoragePrefixesColumn* prefixes, bool confirmed) :
        m_entries(entries), m_prefixes(prefixes), m_confirmed(confirmed) {}

    StorageFacade(const std::unique_ptr<StorageEntriesColumn>& entries,
                  const std::unique_ptr<StoragePrefixesColumn>& prefixes, bool confirmed) :
        StorageFacade(entries.get(), prefixes.get(), confirmed)
    {}

    Prefix_cptr getPrefix(const std::string& prefixId) const;

    void addPrefix(const Prefix_cptr& prefix);

    void updatePrefix(const Prefix_cptr& prefix);

    uint64_t getPrefixesCount() const;

    StorageEntry_cptr getEntry(const std::string& prefix, const std::string& key) const;

    void addEntry(const std::string& prefixId, const std::string& key, const StorageEntry_cptr& entry);

    uint64_t getEntriesCount() const;

    std::vector<TransactionId> getTransasctionsForPrefix(const std::string& prefix, uint32_t page) const;

private:
    StorageEntriesColumn* m_entries;
    StoragePrefixesColumn* m_prefixes;
    bool m_confirmed;
};

}
}
