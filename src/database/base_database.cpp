#include "pch.h"

#include "base_database.h"
#include "columns/default.h"

namespace logpass {
namespace database {

BaseDatabase::BaseDatabase(const DatabaseOptions& options, const std::shared_ptr<Filesystem>& filesystem) :
    EventLoopThread("database"), m_options(options), m_filesystem(filesystem)
{
    m_logger.add_attribute("Class", boost::log::attributes::constant<std::string>("Database"));
    m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(
        m_filesystem->getDatabaseDir().string()));

    m_dbOptions.atomic_flush = true;
    m_dbOptions.create_if_missing = true;
    m_dbOptions.create_missing_column_families = true;
    m_dbOptions.paranoid_checks = true;
    m_dbOptions.write_buffer_size = 1024 * 1024 * 1024;
    m_dbOptions.max_open_files = m_options.maxOpenFiles;
    m_dbOptions.max_write_buffer_number = 20;
    m_dbOptions.max_background_jobs = 4;
    m_dbOptions.max_subcompactions = 4;
}

void BaseDatabase::start(std::vector<rocksdb::ColumnFamilyDescriptor> columns)
{
    // default column is required by rocksdb, add if missing
    auto it = std::find_if(columns.begin(), columns.end(), [](auto& column) {
        return column.name == DefaultColumn::getName();
    });
    if (it == columns.end()) {
        columns.emplace_back(DefaultColumn::getName(), DefaultColumn::getOptions());
    }

    rocksdb::Status status = rocksdb::DB::Open(m_dbOptions, m_filesystem->getDatabaseDir().string(),
                                               columns, &m_handles, &m_db);
    if (!status.ok()) {
        LOG_CLASS(fatal) << "Can't open database, status: " << status.ToString();
        std::terminate();
    }

    EventLoopThread::start();
}

void BaseDatabase::stop()
{
    LOG_CLASS(debug) << "stopping";
    if (m_future.valid()) {
        m_future.get();
    }
    EventLoopThread::stop(); // in this case, thread::stop should be called first
    for (rocksdb::ColumnFamilyHandle* handle : m_handles) {
        m_db->DestroyColumnFamilyHandle(handle);
    }
    m_db->Close();
    delete m_db;
}

void BaseDatabase::load()
{
    for (Column* column : columns()) {
        column->load();
    }

    // validate blockId for every column
    uint32_t blockId = columns().front()->getBlockId();
    for (Column* column : columns()) {
        uint32_t columnBlockId = column->getBlockId();
        if (blockId != columnBlockId) {
            LOG_CLASS(fatal) << "Column: " << column->getName() << " has invalid blockId, it is " << columnBlockId <<
                " but it should be " << blockId;
            std::terminate();
        }
    }
}

void BaseDatabase::clear()
{
    for (auto& column : columns()) {
        column->clear();
    }
}

void BaseDatabase::preload(uint32_t blockId)
{
    for (auto& column : columns()) {
        column->preload(blockId);
    }
}

void BaseDatabase::commit(uint32_t blockId)
{
    LOG_CLASS(debug) << "Commit";

    // prepare commit for database
    rocksdb::WriteBatch batch;
    {
        PerformanceTimer timer("Creating batch", &m_logger);
        for (auto& column : columns()) {
            column->prepare(blockId, batch);
        }
    }

    // finish last commit before doing next
    if (m_future.valid()) {
        m_future.get();
    }

    // write changes to db
    {
        PerformanceTimer timer("Writing changes", &m_logger);
        LOG_CLASS(info) << "Writing " << batch.Count() << " changes, size: " << (batch.GetDataSize() / 1024) << " KB";
        rocksdb::WriteOptions writeOptions;
        writeOptions.disableWAL = true;
        writeOptions.sync = false;
        auto status = m_db->Write(writeOptions, &batch);
        if (!status.ok()) {
            LOG_CLASS(fatal) << "Can't write changes to database: " << status.ToString();
            std::terminate();
        }
    }

    // finish commit
    {
        for (auto& column : columns()) {
            column->commit();
        }
    }

    m_promise = std::promise<void>();
    m_future = m_promise.get_future();
    post([this] {
        {
            PerformanceTimer timer("Flushing", &m_logger);
            rocksdb::FlushOptions flushOptions;
            flushOptions.wait = true;
            auto status = m_db->Flush(flushOptions, m_handles);
            if (!status.ok()) {
                LOG_CLASS(fatal) << "Can't flush database: " << status.ToString();
                std::terminate();
            }
        }

        bool activeCompaction = false;
        std::vector<std::pair<Column*, size_t>> compactionCandidates;
        for (auto& column : columns()) {
            rocksdb::ColumnFamilyMetaData meta = column->getMetaData();
            if (meta.levels[0].files.size() <= kDatabaseRolbackableBlocks) {
                continue;
            }
            bool isCompacting = false;
            for (auto& file : meta.levels[0].files) {
                if (file.being_compacted) {
                    isCompacting = true;
                    break;
                }
            }
            if (isCompacting) {
                activeCompaction = true;
                continue;
            }
            compactionCandidates.push_back(std::make_pair(column, meta.levels[0].files.size()));
        }

        std::sort(compactionCandidates.begin(), compactionCandidates.end(), [](auto& a, auto& b) {
            return a.second > b.second;
        });

        m_promise.set_value();

        if (compactionCandidates.empty()) {
            return;
        }

        size_t files = compactionCandidates[0].second;
        if (activeCompaction && files < kDatabaseRolbackableBlocks + kDatabaseRolbackableBlocks / 4) {
            return;
        }

        // for column family with most L0 files suggest compaction of oldest files but don't
        // remove kDatabaseRolbackableBlocks newest files, they're needed for rollback
        LOG_CLASS(debug) << "Suggesting compaciton for " << compactionCandidates[0].first->getName() <<
            " with " << compactionCandidates[0].second << " files";
        m_db->SuggestPartialL0Compaction(compactionCandidates[0].first->getHandle(), kDatabaseRolbackableBlocks);
    });
}

bool BaseDatabase::rollback(uint32_t blocks)
{
    for (auto& column : columns()) {
        column->clear();
    }

    if (blocks == 0) {
        return true;
    }

    if (m_future.valid()) {
        m_future.get();
    }

    for (auto& columnFamily : m_handles) {
        m_db->SetOptions(columnFamily, { { "disable_auto_compactions", "true" } });
    }

    std::vector<rocksdb::ColumnFamilyMetaData> metas;
    for (auto& column : columns()) {
        metas.push_back(column->getMetaData());
    }

    uint32_t maxRollback = kDatabaseRolbackableBlocks;
    for (auto& meta : metas) {
        if (meta.levels.empty()) {
            maxRollback = 0;
            break;
        }
        maxRollback = std::min<uint32_t>(maxRollback, meta.levels[0].files.size());
        for (uint32_t i = 0; i < maxRollback; ++i) {
            if (meta.levels[0].files[i].being_compacted) {
                maxRollback = i;
                break;
            }
        }
    }

    if (blocks > maxRollback) {
        m_db->EnableAutoCompaction(m_handles);
        LOG_CLASS(debug) << "[DB] Can't rollback " << blocks << " blocks, max rollback is " << maxRollback;
        return false;
    }

    std::vector<std::string> filesToDelete;
    for (uint32_t i = 0; i < blocks; ++i) {
        for (auto& meta : metas) {
            filesToDelete.push_back(meta.levels[0].files[i].name);
        }
    }

    auto status = m_db->DeleteFiles(filesToDelete);
    if (!status.ok()) {
        LOG_CLASS(fatal) << "[DB] Can't delete files during rollback: " << status.ToString();
        std::terminate();
    }

    load();

    m_db->EnableAutoCompaction(m_handles);
    return true;
}

uint32_t BaseDatabase::getMaxRollbackDepth()
{
    if (m_future.valid()) {
        m_future.get();
    }

    std::vector<rocksdb::ColumnFamilyMetaData> metas;
    for (auto& column : columns()) {
        metas.push_back(column->getMetaData());
    }

    uint32_t maxRollback = kDatabaseRolbackableBlocks;
    for (auto& meta : metas) {
        if (meta.levels.empty()) {
            maxRollback = 0;
            break;
        }
        maxRollback = std::min<uint32_t>(maxRollback, meta.levels[0].files.size());
        for (uint32_t i = 0; i < maxRollback; ++i) {
            if (meta.levels[0].files[i].being_compacted) {
                maxRollback = i;
            }
        }
    }

    return maxRollback;
}

json BaseDatabase::getDebugInfo() const
{
    // thread-safe functions
    json j;
    std::string statisticsRaw = m_dbOptions.statistics ? m_dbOptions.statistics->ToString() : "";
    std::vector<std::string> splittedStatistics;
    std::map<std::string, std::string> statistics;
    boost::split(splittedStatistics, statisticsRaw, boost::is_any_of("\n"), boost::token_compress_on);
    for (auto& statistic : splittedStatistics) {
        std::vector<std::string> splittedString;
        boost::split(splittedString, statistic, boost::is_any_of(":"), boost::token_compress_on);
        if (splittedString.size() != 2)
            continue;
        boost::trim(splittedString[0]);
        boost::trim(splittedString[1]);
        statistics[splittedString[0]] = splittedString[1];
    }
    j["statistics"] = statistics;
    j["columns"] = {};
    for (auto& family : m_handles) {
        rocksdb::ColumnFamilyMetaData meta;
        m_db->GetColumnFamilyMetaData(family, &meta);
        j["columns"][meta.name] = {
            {"levels", {}},
            {"file_count", meta.file_count},
            {"size", meta.size},
        };
        for (auto& level : meta.levels) {
            j["columns"][meta.name]["levels"][level.level] = {
                {"files", {}},
                {"file_count", level.files.size()},
                {"size", level.size},
                {"level", level.level}
            };
            for (auto& file : level.files) {
                j["columns"][meta.name]["levels"][level.level]["files"][file.name] = {
                    {"size", file.size},
                    {"smallest_key", base64url::encode(file.smallestkey)},
                    {"largest_key", base64url::encode(file.largestkey)},
                    {"entries", file.num_entries},
                    {"deletions", file.num_deletions},
                    {"is_compacted", file.being_compacted},
                    {"creation_time", file.file_creation_time},
                };
            }
        }
    }

    std::vector<json> json_threads;
    std::vector<rocksdb::ThreadStatus> threads;
    m_dbOptions.env->GetThreadList(&threads);
    for (auto& thread : threads) {
        json_threads.push_back({
            {"type", rocksdb::ThreadStatus::GetThreadTypeName(thread.thread_type)},
            {"operation", rocksdb::ThreadStatus::GetOperationName(thread.operation_type)},
            {"stage", rocksdb::ThreadStatus::GetOperationStageName(thread.operation_stage)},
            {"db", thread.db_name},
            {"column_family", thread.cf_name},
            {"time_elapsed", thread.op_elapsed_micros},
                               });
    }
    j["threads"] = json_threads;
    return j;
}

}
}
