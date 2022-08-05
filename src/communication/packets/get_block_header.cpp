#include "pch.h"
#include "get_block_header.h"

#include <blockchain/blockchain.h>
#include <communication/session.h>
#include <communication/connection/connection.h>

namespace logpass {

Packet_ptr GetBlockHeaderPacket::create(const std::vector<std::pair<uint32_t, Hash>>& blockIdsAndHashes)
{
    ASSERT(blockIdsAndHashes.size() > 0 && blockIdsAndHashes.size() <= MAX_BLOCKS);
    auto packet = std::make_shared<GetBlockHeaderPacket>();
    packet->m_blockIdsAndHashes = blockIdsAndHashes;
    return packet;
}

void GetBlockHeaderPacket::serializeRequestBody(Serializer& s)
{
    s(m_blockIdsAndHashes);
}

void GetBlockHeaderPacket::serializeResponseBody(Serializer& s)
{
    s.serializeOptional(m_blockHeader);
}

bool GetBlockHeaderPacket::validateRequest()
{
    if (m_blockIdsAndHashes.size() == 0 && m_blockIdsAndHashes.size() > MAX_BLOCKS)
        return false;
    std::set<Hash> uniqueHashes;
    for (auto& [blockId, hash] : m_blockIdsAndHashes) {
        if (blockId == 0)
            return false;
        uniqueHashes.insert(hash);
    }
    if (uniqueHashes.size() != m_blockIdsAndHashes.size())
        return false;
    return true;
}

bool GetBlockHeaderPacket::validateResponse()
{
    if (!m_blockHeader)
        return true;
    Hash blockHash = m_blockHeader->getHash();
    for (auto& [blockId, hash] : m_blockIdsAndHashes) {
        if (hash == blockHash)
            return false;
    }
    Hash prevBlockHash = m_blockHeader->getPrevHeaderHash();
    for (auto& [blockId, hash] : m_blockIdsAndHashes) {
        if (hash == prevBlockHash)
            return true;
    }
    return false;
}

void GetBlockHeaderPacket::executeRequest(const PacketExecutionParams& params)
{
    std::set<Hash> uniqueHashes;
    for (auto& [blockId, hash] : m_blockIdsAndHashes)
        uniqueHashes.insert(hash);

    auto activeBranch = params.blockTree->getActiveBranch();
    for (auto& [blockId, hash] : m_blockIdsAndHashes) {
        // first try to find block header in active branch of BlockTree
        auto it = std::find_if(activeBranch.begin(), activeBranch.end(), [&](auto& node) {
            return node.getPrevHeaderHash() == hash;
        });
        if (it != activeBranch.end() && !uniqueHashes.contains(it->block->getHeaderHash())) {
            m_blockHeader = it->block->getBlockHeader();
            break;
        }

        // try to find block in database
        auto blockHeader = params.database->blocks.getNextBlockHeader(blockId);
        if (!blockHeader)
            continue;
        if (blockHeader->getPrevHeaderHash() != hash)
            continue;
        if (blockId + blockHeader->getSkippedBlocks() + 1 != blockHeader->getId())
            continue;
        if (uniqueHashes.contains(blockHeader->getHash()))
            break;
        m_blockHeader = blockHeader;
        break;
    }
}

void GetBlockHeaderPacket::executeResponse(const PacketExecutionParams& params)
{
    return params.session->onBlockHeader(m_blockHeader);
}

}
