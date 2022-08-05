#include "pch.h"
#include "new_blocks.h"

#include <blockchain/blockchain.h>
#include <communication/connection/connection.h>
#include <communication/session.h>
#include <database/database.h>

namespace logpass {

Packet_ptr NewBlocksPacket::create(const BlockHeader_cptr& lastBlockHeader)
{
    auto packet = std::make_shared<NewBlocksPacket>();
    packet->m_lastBlockHeader = lastBlockHeader;
    return packet;
}

void NewBlocksPacket::serializeRequestBody(Serializer& s)
{
    s(m_lastBlockHeader);
}

bool NewBlocksPacket::validateRequest()
{
    if (m_lastBlockHeader == nullptr) {
        return false;
    }
    return true;
}

void NewBlocksPacket::executeRequest(const PacketExecutionParams& params)
{
    params.session->onNewBlocks(m_lastBlockHeader);
}

}
