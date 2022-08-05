#pragma once

#include "packet.h"
#include <blockchain/block/pending_block.h>

namespace logpass {

// packet used to get block header after one of blocks header with given id and hash
class GetBlockPacket : public PacketWithResponse {
public:
    static constexpr uint8_t TYPE = 0x08;
    static constexpr size_t MAX_TRANSACTION_IDS = 32;
    static constexpr size_t MAX_TRANSACTIONS = 4096;
    static constexpr size_t MAX_TRANSACTIONS_SIZE = kNetworkMaxPacketSize - 1024;

    GetBlockPacket() : PacketWithResponse(TYPE) {}

    static Packet_ptr create(PendingBlock::Status status, const PendingBlock_ptr& pendingBlock);

protected:
    void serializeRequestBody(Serializer& s) override;
    void serializeResponseBody(Serializer& s) override;

    bool validateRequest() override;
    bool validateResponse() override;

    void executeRequest(const PacketExecutionParams& params) override;
    void executeResponse(const PacketExecutionParams& params) override;

protected:
    PendingBlock_ptr m_pendingBlock;

    // request
    uint32_t m_blockId;
    Hash m_headerHash;
    PendingBlock::Status m_status;
    Hash m_bodyHash;
    std::vector<std::pair<uint32_t, Hash>> m_transactionIdsHashes;
    std::set<TransactionId> m_transactionIds;

    // response
    bool m_expired = false;
    BlockBody_cptr m_blockBody;
    std::vector<BlockTransactionIds_cptr> m_blockTransactionIds;
    std::vector<Transaction_cptr> m_transactions;
};

}
