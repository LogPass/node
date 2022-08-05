#pragma once

#include "packet.h"

#include <blockchain/block/block_header.h>

namespace logpass {

class NewTransactionsPacket : public Packet {
public:
    constexpr static uint8_t TYPE = 0x05;
    constexpr static size_t MAX_TRANSACTION_IDS = 16'384;

    NewTransactionsPacket() : Packet(TYPE) {}

    static Packet_ptr create(const std::set<TransactionId>& transactionIds);

protected:
    void serializeRequestBody(Serializer& s) override;

    bool validateRequest() override;

    void executeRequest(const PacketExecutionParams& params) override;

private:
    std::set<TransactionId> m_transactionIds;
};

}
