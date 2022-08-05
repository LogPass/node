#include "pch.h"
#include "blocks.h"

namespace logpass {
namespace database {

rocksdb::ColumnFamilyOptions BlocksColumn::getOptions()
{
    rocksdb::ColumnFamilyOptions columnOptions = Column::getOptions();
    return columnOptions;
}

BlockHeader_cptr BlocksColumn::getBlockHeader(uint32_t blockId, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_blocks.find(blockId);
        if (it != m_blocks.end()) {
            return it->second->getBlockHeader();
        }
    }

    {
        // check latestBlocks (cache), maybe it's there
        std::shared_lock lock(m_mutex);
        auto& latestBlocks = state(confirmed).latestBlocks;
        auto it = latestBlocks.find(blockId);
        if (it != latestBlocks.end()) {
            return it->second.first;
        }
    }

    auto s = get<uint32_t>(boost::endian::endian_reverse(blockId));
    if (!s) {
        return nullptr;
    }

    BlockHeader_ptr blockHeader = std::make_shared<BlockHeader>();
    (*s)(blockHeader);
    return blockHeader;
}

BlockHeader_cptr BlocksColumn::getNextBlockHeader(uint32_t blockId, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_blocks.find(blockId);
        if (it != m_blocks.end()) {
            ++it;
            if (it != m_blocks.end()) {
                return it->second->getBlockHeader();
            }
        }
    }

    {
        // check latestBlocks (cache), maybe it's there
        std::shared_lock lock(m_mutex);
        auto& latestBlocks = state(confirmed).latestBlocks;
        auto it = latestBlocks.find(blockId);
        if (it != latestBlocks.end()) {
            ++it;
            if (it != latestBlocks.end()) {
                return it->second.first;
            }
        }
    }


    rocksdb::Iterator* it = m_db->NewIterator(rocksdb::ReadOptions(), m_handle);
    Serializer key;
    key.put<uint32_t>(boost::endian::endian_reverse(blockId + 1));
    it->Seek(key);
    if (!it->Valid()) {
        delete it;
        return nullptr;
    }

    Serializer s(it->value());
    auto blockHeader = std::make_shared<BlockHeader>();
    s(blockHeader);
    delete it;
    return blockHeader;
}

BlockBody_cptr BlocksColumn::getBlockBody(uint32_t blockId, bool confirmed) const
{
    if (!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_blocks.find(blockId);
        if (it != m_blocks.end()) {
            return it->second->getBlockBody();
        }
    }

    {
        // check latestBlocks (cache), maybe it's there
        std::shared_lock lock(m_mutex);
        auto& latestBlocks = state(confirmed).latestBlocks;
        auto it = latestBlocks.find(blockId);
        if (it != latestBlocks.end()) {
            return it->second.second;
        }
    }

    auto s = get<uint32_t, uint8_t>(boost::endian::endian_reverse(blockId), 'B');
    if (!s) {
        return nullptr;
    }

    BlockBody_ptr blockBody = std::make_shared<BlockBody>();
    (*s)(blockBody);
    return blockBody;
}

BlockTransactionIds_cptr BlocksColumn::getBlockTransactionIds(uint32_t blockId, uint8_t chunk, bool confirmed) const
{
    if(!confirmed) {
        std::shared_lock lock(m_mutex);
        auto it = m_blocks.find(blockId);
        if (it != m_blocks.end()) {
            auto transactionIds = it->second->getBlockTransactionIds();
            if (chunk >= transactionIds.size()) {
                return nullptr;
            }
            return transactionIds[chunk];
        }
    }

    auto s = get<uint32_t, uint8_t, uint8_t>(boost::endian::endian_reverse(blockId), 'T', chunk);
    if (!s) {
        return nullptr;
    }

    BlockTransactionIds_ptr blockTransactionIds = std::make_shared<BlockTransactionIds>();
    (*s)(blockTransactionIds);
    return blockTransactionIds;
}

std::map<uint32_t, std::pair<BlockHeader_cptr, BlockBody_cptr>> BlocksColumn::getLatestBlocks(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    return state(confirmed).latestBlocks;
}

BlockHeader_cptr BlocksColumn::getLatestBlockHeader(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    auto& latestBlocks = state(confirmed).latestBlocks;
    auto it = latestBlocks.rbegin();
    if (it != latestBlocks.rend()) {
        return it->second.first;
    }
    return nullptr;
}

MinersQueue BlocksColumn::getMinersQueue(bool confirmed) const
{
    std::shared_lock lock(m_mutex);
    MinersQueue queue;
    auto& latestBlocks = state(confirmed).latestBlocks;
    for (auto& [blockId, blockPair] : latestBlocks | views::reverse) {
        auto blockQueue = blockPair.first->getNextMiners();
        queue.insert(queue.begin(), blockQueue.begin(), blockQueue.end());
        if (queue.size() >= kMinersQueueSize) {
            break;
        }
    }
    while (queue.size() > kMinersQueueSize) {
        queue.erase(queue.begin(), std::next(queue.begin(), queue.size() - kMinersQueueSize));
    }
    return queue;
}

void BlocksColumn::addBlock(const Block_cptr& block)
{
    ASSERT(m_blocks.empty());
    ASSERT(!getBlockHeader(block->getId(), false));
    ASSERT(block->getId() == 1 ||
           block->getId() == getLatestBlockHeader(false)->getId() + block->getSkippedBlocks() + 1);
    ASSERT(block->getDepth() == 1 ||
           block->getDepth() == getLatestBlockHeader(false)->getDepth() + 1);
    std::unique_lock lock(m_mutex);
    m_blocks[block->getId()] = block;
    state().blocks += 1;
    auto& latestBlocks = state().latestBlocks;
    latestBlocks[block->getId()] = { block->getBlockHeader(), block->getBlockBody() };
    while (latestBlocks.size() > LATEST_BLOCKS_SIZE) {
        latestBlocks.erase(latestBlocks.begin());
    }
}

void BlocksColumn::load()
{
    std::unique_lock lock(m_mutex);
    ASSERT(m_blocks.empty());
    StatefulColumn::load();
}

void BlocksColumn::prepare(uint32_t blockId, rocksdb::WriteBatch& batch)
{
    std::shared_lock lock(m_mutex);
    ASSERT(state().blockId < blockId);

    StatefulColumn::prepare(blockId, batch);

    // serialize blocks
    for (auto& [blockIdLE, block] : m_blocks) {
        // big endian blockId
        uint32_t blockIdBE = boost::endian::endian_reverse(blockIdLE);
        // header
        Serializer blockHeader;
        blockHeader.serialize(block->getBlockHeader());
        put<uint32_t>(batch, blockIdBE, blockHeader);
        // body
        Serializer blockBody;
        blockBody.serialize(block->getBlockBody());
        put<uint32_t, uint8_t>(batch, blockIdBE, 'B', blockBody);
        // transaction ids
        for (uint8_t index = 0; auto & blockTransactionIds : block->getBlockTransactionIds()) {
            Serializer serializer;
            serializer(blockTransactionIds);
            put<uint32_t, uint8_t, uint8_t>(batch, blockIdBE, 'T', index++, serializer);
        }
    }
}

void BlocksColumn::commit()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::commit();
    m_blocks.clear();
}

void BlocksColumn::clear()
{
    std::unique_lock lock(m_mutex);
    StatefulColumn::clear();
    m_blocks.clear();
}

}
}
