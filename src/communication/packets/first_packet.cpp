#include "pch.h"

#include "first_packet.h"
#include <blockchain/blockchain.h>
#include <communication/connection/connection.h>
#include <communication/session.h>
#include <database/database.h>

namespace logpass {

Packet_ptr FirstPacket::create(const BlockHeader_cptr& latestBlockHeader)
{
    auto packet = std::make_shared<FirstPacket>();
    packet->m_latestBlockHeader = latestBlockHeader;
    return packet;
}

void FirstPacket::serializeRequestBody(Serializer& s)
{
    s(m_latestBlockHeader);
}

bool FirstPacket::validateRequest()
{
    if (m_latestBlockHeader == nullptr) {
        return false;
    }
    return true;
}

void FirstPacket::executeRequest(const PacketExecutionParams& params)
{
    params.session->onFirstPacket(m_latestBlockHeader);
}

}
