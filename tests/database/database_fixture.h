#pragma once

#include <database/database.h>
#include <filesystem/filesystem.h>

namespace logpass {

template<class Database = logpass::Database>
struct DatabaseFixture {
    DatabaseFixture(const DatabaseOptions& options = DatabaseOptions())
    {
        fs = Filesystem::createTemporaryFilesystem();
        db = SharedThread<Database>(options, fs);
    }

    ~DatabaseFixture()
    {
        db.reset();
        fs.reset();
    }

    DatabaseFixture(const DatabaseFixture&) = delete;
    DatabaseFixture& operator=(const DatabaseFixture&) = delete;

    void reinitialize()
    {
        db.reset();
        db = SharedThread<Database>(DatabaseOptions(), fs);
    }

    SharedThread<Filesystem> fs;
    SharedThread<Database> db;
};

}
