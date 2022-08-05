#pragma once

#include "packet.h"

#include <blockchain/block/block_header.h>

namespace logpass {

class FirstPacket : public Packet {
public:
    constexpr static uint8_t TYPE = 0x01;

    FirstPacket() : Packet(TYPE) {}

    static Packet_ptr create(const BlockHeader_cptr& latestBlockHeader);

protected:
    void serializeRequestBody(Serializer& s) override;

    bool validateRequest() override;

    void executeRequest(const PacketExecutionParams& params) override;

private:
    BlockHeader_cptr m_latestBlockHeader;
};

}
