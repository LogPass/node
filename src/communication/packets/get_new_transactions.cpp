#include "pch.h"
#include "get_new_transactions.h"

#include <blockchain/blockchain.h>
#include <blockchain/block/block.h>
#include <blockchain/transactions/transaction.h>
#include <communication/connection/connection.h>
#include <communication/session.h>

namespace logpass {

Packet_ptr GetNewTransactionsPacket::create(const std::set<TransactionId>& transactionIds)
{
    ASSERT(!transactionIds.empty());
    auto packet = std::make_shared<GetNewTransactionsPacket>();
    packet->m_transactionIds = transactionIds;
    return packet;
}

void GetNewTransactionsPacket::serializeRequestBody(Serializer& s)
{
    s(m_transactionIds);
}

void GetNewTransactionsPacket::serializeResponseBody(Serializer& s)
{
    s(m_transactions);
}

bool GetNewTransactionsPacket::validateRequest()
{
    if (m_transactionIds.empty()) {
        return false;
    }

    uint32_t transactionsSize = 0;
    for (auto& transactionId : m_transactionIds) {
        transactionsSize += transactionId.getSize();
    }

    if (transactionsSize > kNetworkMaxPacketSize - 1024) {
        return false;
    }

    for (auto& transactionId : m_transactionIds) {
        if (transactionId.getSize() == 0 || transactionId.getSize() > kTransactionMaxSize) {
            return false;
        }
    }

    return true;
}

bool GetNewTransactionsPacket::validateResponse()
{
    std::set<TransactionId> uniqueTransactions;
    for (auto& transaction : m_transactions) {
        if (!m_transactionIds.contains(transaction->getId())) {
            return false;
        }
        if (uniqueTransactions.contains(transaction->getId())) {
            return false;
        }
        uniqueTransactions.insert(transaction->getId());
    }
    return true;
}

void GetNewTransactionsPacket::executeRequest(const PacketExecutionParams& params)
{
    auto transactionIds = std::vector<TransactionId>(m_transactionIds.begin(), m_transactionIds.end());
    auto transactions = params.blockchain->getTransactions(transactionIds);
    for (auto& transaction : transactions) {
        m_transactions.push_back(transaction);
    }
}

void GetNewTransactionsPacket::executeResponse(const PacketExecutionParams& params)
{
    std::map<TransactionId, Transaction_cptr> transactions;
    for (auto& transactionId : m_transactionIds) {
        transactions[transactionId] = nullptr;
    }
    for (auto& transaction : m_transactions) {
        transactions[transaction->getId()] = transaction;
    }
    params.session->onTransactions(transactions);
}

}
