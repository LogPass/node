#pragma once

#include <filesystem/filesystem.h>

#include "database_options.h"
#include "database/columns/column.h"

namespace logpass {
namespace database {

// database with no colums defined
class BaseDatabase : public EventLoopThread {
protected:
    BaseDatabase(const DatabaseOptions& options, const std::shared_ptr<Filesystem>& filesystem);

    // starts the database
    void start(std::vector<rocksdb::ColumnFamilyDescriptor> columns);

    // stops database, must be called before constructor
    virtual void stop() override;

    // loads columns state, must be called after start
    void load();

    // returns list of columns
    virtual std::vector<Column*> columns() const = 0;

public:
    // clears temporary chances
    void clear();
    // preload data
    void preload(uint32_t blockId);
    // commits changes
    void commit(uint32_t blockId);
    // rollbacks given number of blocks
    bool rollback(uint32_t blocks);
    // returns max rollback depth
    uint32_t getMaxRollbackDepth();
    // returns debug info about database
    json getDebugInfo() const;

protected:
    const DatabaseOptions m_options;
    const std::shared_ptr<Filesystem> m_filesystem;
    mutable Logger m_logger;

    rocksdb::DB* m_db = nullptr;
    rocksdb::Options m_dbOptions;
    std::vector<rocksdb::ColumnFamilyHandle*> m_handles;
    std::promise<void> m_promise;
    std::future<void> m_future;
};

}
}
