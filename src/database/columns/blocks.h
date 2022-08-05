#pragma once

#include "stateful_column.h"
#include <blockchain/block/block.h>

namespace logpass {
namespace database {

struct BlocksColumnState : public ColumnState {
    uint32_t blocks = 0;
    std::map<uint32_t, std::pair<BlockHeader_cptr, BlockBody_cptr>> latestBlocks;

    void serialize(Serializer& s)
    {
        ColumnState::serialize(s);
        s(blocks);
        s(latestBlocks);
    }
};

// keeps blocks
class BlocksColumn : public StatefulColumn<BlocksColumnState> {
public:
    constexpr static size_t LATEST_BLOCKS_SIZE = kMinersQueueSize + kDatabaseRolbackableBlocks;

    using StatefulColumn::StatefulColumn;

    // returns name of the column
    static std::string getName()
    {
        return "blocks";
    }

    // returns options for column
    static rocksdb::ColumnFamilyOptions getOptions();

    // returns block header with given id, may return nullptr
    BlockHeader_cptr getBlockHeader(uint32_t blockId, bool confirmed) const;
    // returns block header after block with given id, may return nullptr
    BlockHeader_cptr getNextBlockHeader(uint32_t blockId, bool confirmed) const;
    // returns block body with given id, may return nullptr
    BlockBody_cptr getBlockBody(uint32_t blockId, bool confirmed) const;
    // returns block transaction ids with given id and chunk, may return nullptr
    BlockTransactionIds_cptr getBlockTransactionIds(uint32_t blockId, uint8_t chunk, bool confirmed) const;

    // returns latest blocks
    std::map<uint32_t, std::pair<BlockHeader_cptr, BlockBody_cptr>> getLatestBlocks(bool confirmed) const;
    // returns latest block header
    BlockHeader_cptr getLatestBlockHeader(bool confirmed) const;
    // returns mining queue
    MinersQueue getMinersQueue(bool confirmed) const;

    // adds block
    void addBlock(const Block_cptr& block);

    // loads state
    void load() override;
    // 
    void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override;
    //
    void commit() override;
    // removes temporary changes
    void clear() override;

private:
    std::map<uint32_t, Block_cptr> m_blocks;
};

}
}
