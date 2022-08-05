#pragma once

#include "packet.h"

#include <blockchain/block/block_header.h>

namespace logpass {

class NewBlocksPacket : public Packet {
public:
    constexpr static uint8_t TYPE = 0x04;

    NewBlocksPacket() : Packet(TYPE) {}

    static Packet_ptr create(const BlockHeader_cptr& lastBlockHeader);

protected:
    void serializeRequestBody(Serializer& s) override;

    bool validateRequest() override;

    void executeRequest(const PacketExecutionParams& params) override;

private:
    BlockHeader_cptr m_lastBlockHeader;
};

}
