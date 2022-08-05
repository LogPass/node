#include "pch.h"
#include "new_transactions.h"

#include <blockchain/blockchain.h>
#include <communication/connection/connection.h>
#include <communication/session.h>
#include <database/database.h>

namespace logpass {

Packet_ptr NewTransactionsPacket::create(const std::set<TransactionId>& transactionIds)
{
    ASSERT(!transactionIds.empty() && transactionIds.size() <= MAX_TRANSACTION_IDS);
    auto packet = std::make_shared<NewTransactionsPacket>();
    packet->m_transactionIds = transactionIds;
    return packet;
}

void NewTransactionsPacket::serializeRequestBody(Serializer& s)
{
    s(m_transactionIds);
}

bool NewTransactionsPacket::validateRequest()
{
    if (m_transactionIds.empty()) {
        return false;
    }
    if (m_transactionIds.size() > MAX_TRANSACTION_IDS) {
        return false;
    }
    for (auto& transactionId : m_transactionIds) {
        if (transactionId.getSize() == 0 || transactionId.getSize() > kTransactionMaxSize) {
            return false;
        }
    }
    return true;
}

void NewTransactionsPacket::executeRequest(const PacketExecutionParams& params)
{
    params.session->onNewTransactionIds(m_transactionIds);
}

}
