#pragma once

#include <filesystem/filesystem.h>

#include "base_database.h"
#include "columns.h"
#include "confirmed_database.h"
#include "database_options.h"
#include "unconfirmed_database.h"

namespace logpass {

class Database : public database::BaseDatabase {
    friend class SharedThread<Database>;

protected:
    Database(const DatabaseOptions& options, const std::shared_ptr<Filesystem>& filesystem);
    void start() override;
    void stop() override;

    std::vector<database::Column*> columns() const override
    {
        return m_columns.all();
    }

public:
    const ConfirmedDatabase& confirmed() const
    {
        return *(m_confirmedDatabase.get());
    }

    UnconfirmedDatabase& unconfirmed()
    {
        return *(m_unconfirmedDatabase.get());
    }

    const UnconfirmedDatabase& unconfirmed() const
    {
        return *(m_unconfirmedDatabase.get());
    }

protected:
    database::Columns m_columns;
    std::unique_ptr<const ConfirmedDatabase> m_confirmedDatabase;
    std::unique_ptr<UnconfirmedDatabase> m_unconfirmedDatabase;
};

}
