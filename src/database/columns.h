#pragma once

#include "columns/blocks.h"
#include "columns/default.h"
#include "columns/miners.h"
#include "columns/storage_entries.h"
#include "columns/storage_prefixes.h"
#include "columns/transaction_hashes.h"
#include "columns/transactions.h"
#include "columns/user_history.h"
#include "columns/user_sponsors.h"
#include "columns/user_updates.h"
#include "columns/users.h"

namespace logpass {
namespace database {

struct Columns {
    std::unique_ptr<database::BlocksColumn> blocks;
    std::unique_ptr<database::DefaultColumn> defaultColumn;
    std::unique_ptr<database::MinersColumn> miners;
    std::unique_ptr<database::StorageEntriesColumn> storageEntries;
    std::unique_ptr<database::StoragePrefixesColumn> storagePrefixes;
    std::unique_ptr<database::TransactionHashesColumn> transactionBodies;
    std::unique_ptr<database::TransactionsColumn> transactions;
    std::unique_ptr<database::UserHistoryColumn> userHistory;
    std::unique_ptr<database::UserSponsorsColumn> userSponsors;
    std::unique_ptr<database::UserUpdatesColumn> userUpdates;
    std::unique_ptr<database::UsersColumn> users;

    std::vector<database::Column*> all() const
    {
        return {
            blocks.get(), defaultColumn.get(), miners.get(), storageEntries.get(),
            storagePrefixes.get(), transactionBodies.get(), transactions.get(), userHistory.get(),
            userSponsors.get(), userUpdates.get(), users.get()
        };
    }
};

}
}
