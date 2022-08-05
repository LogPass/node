#include "pch.h"

#include "column.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions Column::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions;
    columnOptions.num_levels = 6;
    columnOptions.compaction_style = rocksdb::kCompactionStyleLevel;
    columnOptions.level_compaction_dynamic_level_bytes = false;
    columnOptions.level0_file_num_compaction_trigger = kDatabaseRolbackableBlocks * 5;
    columnOptions.level0_slowdown_writes_trigger = kDatabaseRolbackableBlocks * 5;
    columnOptions.level0_stop_writes_trigger = kDatabaseRolbackableBlocks * 5;
    columnOptions.target_file_size_base = 64 * 1024 * 1024; // 64 MB
    columnOptions.target_file_size_multiplier = 2;
    columnOptions.max_bytes_for_level_base = 2048ull * 1048576ull; // 2 GB
    columnOptions.max_bytes_for_level_multiplier = 1;
    columnOptions.max_bytes_for_level_multiplier_additional = { 1, 1, 2, 5, 5, 10 };
    return columnOptions;
}

}
}
