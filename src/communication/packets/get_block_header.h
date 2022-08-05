#pragma once

#include "packet.h"
#include <blockchain/block/block_header.h>

namespace logpass {

// packet used to get block header after one of blocks header with given id and hash
class GetBlockHeaderPacket : public PacketWithResponse {
public:
    constexpr static uint8_t TYPE = 0x07;
    constexpr static size_t MAX_BLOCKS = 100;

    GetBlockHeaderPacket() : PacketWithResponse(TYPE) {}

    static Packet_ptr create(const std::vector<std::pair<uint32_t, Hash>>& blockIdsAndHashes);

protected:
    void serializeRequestBody(Serializer& s) override;
    void serializeResponseBody(Serializer& s) override;

    bool validateRequest() override;
    bool validateResponse() override;

    void executeRequest(const PacketExecutionParams& params) override;
    void executeResponse(const PacketExecutionParams& params) override;

protected:
    // request
    std::vector<std::pair<uint32_t, Hash>> m_blockIdsAndHashes;

    // response
    BlockHeader_cptr m_blockHeader;
};

}
