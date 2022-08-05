#include "pch.h"
#include "packet.h"

#include "first_packet.h"
#include "get_block_header.h"
#include "get_block.h"
#include "get_new_transactions.h"
#include "new_blocks.h"
#include "new_transactions.h"

#include <communication/connection/connection.h>

namespace logpass {

Packet::Packet(uint8_t type) : m_type(type)
{

}

Packet_ptr Packet::load(Serializer& s, uint32_t id)
{
    static const std::map<uint8_t, std::function<Packet_ptr()>> packetSerializers = {
        {FirstPacket::TYPE, [] { return std::make_shared<FirstPacket>(); }},
        {GetBlockHeaderPacket::TYPE, [] { return std::make_shared<GetBlockHeaderPacket>(); }},
        {GetBlockPacket::TYPE, [] { return std::make_shared<GetBlockPacket>(); }},
        {GetNewTransactionsPacket::TYPE, [] { return std::make_shared<GetNewTransactionsPacket>(); }},
        {NewBlocksPacket::TYPE, [] { return std::make_shared<NewBlocksPacket>(); }},
        {NewTransactionsPacket::TYPE, [] { return std::make_shared<NewTransactionsPacket>(); }},
    };

    ASSERT(packetSerializers.count(0x00) == 0); // packet 0x00 is reserved

    uint8_t transactionType = s.peek<uint8_t>();
    auto packetIterator = packetSerializers.find(transactionType);
    if (packetIterator == packetSerializers.end())
        THROW_SERIALIZER_EXCEPTION("Invalid packet type");

    auto packet = packetIterator->second();
    packet->serializeRequest(s);
    packet->m_id = id;
    return packet;
}

void Packet::execute(const PacketExecutionParams& params)
{
    if (!hasResponse()) {
        ASSERT(!m_executedRequest);
        executeRequest(params);
        m_executedRequest = true;
        if (requiresResponse()) {
            m_hasResponse = true;
            params.connection->send(shared_from_this());
        }
        return;
    }

    ASSERT(hasResponse() && requiresResponse() && !m_executedResponse);
    executeResponse(params);
    m_executedResponse = true;
}

void Packet::serializeRequest(Serializer& s)
{
    s(m_type);
    serializeRequestBody(s);
    ASSERT(s.reader() || validateRequest());
    if (s.reader() && !validateRequest())
        THROW_SERIALIZER_EXCEPTION("Validation of request failed");
}

void Packet::serializeResponse(Serializer& s)
{
    //ASSERT(m_id == 0 || m_hasResponse);
    ASSERT(requiresResponse());
    serializeResponseBody(s);
    ASSERT(s.reader() || validateResponse());
    if (s.reader() && !validateResponse())
        THROW_SERIALIZER_EXCEPTION("Validation of response failed");
    m_hasResponse = true;
}

}
