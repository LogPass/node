#include "pch.h"

#include "database.h"

using namespace logpass::database;

namespace logpass {

Database::Database(const DatabaseOptions& options, const std::shared_ptr<Filesystem>& filesystem) :
    BaseDatabase(options, filesystem)
{
    m_dbOptions.enable_thread_tracking = true;
    m_dbOptions.statistics = rocksdb::CreateDBStatistics();
    m_dbOptions.env->SetBackgroundThreads(4, rocksdb::Env::HIGH);
    m_dbOptions.env->SetBackgroundThreads(4, rocksdb::Env::LOW);
}

void Database::start()
{
    const std::vector<rocksdb::ColumnFamilyDescriptor> columns = {
        {BlocksColumn::getName(), BlocksColumn::getOptions()},
        {DefaultColumn::getName(), DefaultColumn::getOptions()},
        {MinersColumn::getName(), MinersColumn::getOptions()},
        {StorageEntriesColumn::getName(), StorageEntriesColumn::getOptions()},
        {StoragePrefixesColumn::getName(), StoragePrefixesColumn::getOptions()},
        {TransactionHashesColumn::getName(), TransactionHashesColumn::getOptions()},
        {TransactionsColumn::getName(), TransactionsColumn::getOptions()},
        {UserHistoryColumn::getName(), UserHistoryColumn::getOptions()},
        {UserSponsorsColumn::getName(), UserSponsorsColumn::getOptions()},
        {UserUpdatesColumn::getName(), UserUpdatesColumn::getOptions()},
        {UsersColumn::getName(), UsersColumn::getOptions()}
    };

    BaseDatabase::start(columns);

    m_columns.blocks = std::make_unique<BlocksColumn>(m_db, m_handles[0]);
    m_columns.defaultColumn = std::make_unique<DefaultColumn>(m_db, m_handles[1]);
    m_columns.miners = std::make_unique<MinersColumn>(m_db, m_handles[2]);
    m_columns.storageEntries = std::make_unique<StorageEntriesColumn>(m_db, m_handles[3]);
    m_columns.storagePrefixes = std::make_unique<StoragePrefixesColumn>(m_db, m_handles[4]);
    m_columns.transactionBodies = std::make_unique<TransactionHashesColumn>(m_db, m_handles[5]);
    m_columns.transactions = std::make_unique<TransactionsColumn>(m_db, m_handles[6]);
    m_columns.userHistory = std::make_unique<UserHistoryColumn>(m_db, m_handles[7]);
    m_columns.userSponsors = std::make_unique<UserSponsorsColumn>(m_db, m_handles[8]);
    m_columns.userUpdates = std::make_unique<UserUpdatesColumn>(m_db, m_handles[9]);
    m_columns.users = std::make_unique<UsersColumn>(m_db, m_handles[10]);

    m_confirmedDatabase = std::unique_ptr<ConfirmedDatabase>(new ConfirmedDatabase{ {
                .blocks = BlocksFacade(m_columns.blocks, m_columns.transactions, true),
                .miners = MinersFacade(m_columns.miners, true),
                .state = StateFacade(m_columns.defaultColumn, true),
                .storage = StorageFacade(m_columns.storageEntries, m_columns.storagePrefixes, true),
                .transactions = TransactionsFacade(m_columns.transactions, m_columns.transactionBodies, true),
                .users = UsersFacade(m_columns.users, m_columns.userHistory, m_columns.userSponsors,
                                     m_columns.userUpdates, true)
                                                             } });

    m_unconfirmedDatabase = std::unique_ptr<UnconfirmedDatabase>(new UnconfirmedDatabase{ {
                .blocks = BlocksFacade(m_columns.blocks, m_columns.transactions, false),
                .miners = MinersFacade(m_columns.miners, false),
                .state = StateFacade(m_columns.defaultColumn, false),
                .storage = StorageFacade(m_columns.storageEntries, m_columns.storagePrefixes, false),
                .transactions = TransactionsFacade(m_columns.transactions, m_columns.transactionBodies, false),
                .users = UsersFacade(m_columns.users, m_columns.userHistory, m_columns.userSponsors,
                                     m_columns.userUpdates, false)
                                                                 } });

    load();
}

void Database::stop()
{
    m_unconfirmedDatabase.reset();
    m_confirmedDatabase.reset();
    BaseDatabase::stop();
}

}
