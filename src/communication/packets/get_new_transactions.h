#pragma once

#include "packet.h"
#include <blockchain/block/pending_block.h>

namespace logpass {

// packet used to get block header after one of blocks header with given id and hash
class GetNewTransactionsPacket : public PacketWithResponse {
public:
    static constexpr uint8_t TYPE = 0x09;
    static constexpr size_t MAX_TRANSACTION_IDS = 16;
    static constexpr size_t MAX_TRANSACTIONS = 1024;

    GetNewTransactionsPacket() : PacketWithResponse(TYPE) {}

    static Packet_ptr create(const std::set<TransactionId>& transactionIds);

protected:
    void serializeRequestBody(Serializer& s) override;
    void serializeResponseBody(Serializer& s) override;

    bool validateRequest() override;
    bool validateResponse() override;

    void executeRequest(const PacketExecutionParams& params) override;
    void executeResponse(const PacketExecutionParams& params) override;

protected:
    // request
    std::set<TransactionId> m_transactionIds;

    // response
    std::vector<Transaction_cptr> m_transactions;
};

}
