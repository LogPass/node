#pragma once

#include <blockchain/blockchain.h>
#include <database/database.h>

#include "connection/connection.h"
#include "packets/packet.h"
#include "session_data.h"
#include "shared_transaction_ids.h"

namespace logpass {

class SessionException : public Exception {
    using Exception::Exception;
};

// not thread-safe
class Session {
public:
    Session(const MinerId& minerId, const std::shared_ptr<Blockchain>& blockchain,
            const std::shared_ptr<const Database>& database);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void onConnected(Connection* connection, const BlockHeader_cptr& latestBlockHeader);
    void onDisconnected();
    bool onPacket(const Packet_ptr& packet);
    void onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch);
    void onNewTransactions(const std::vector<Transaction_cptr>& transactions);
    void onPeriodicalCheck();

    void onFirstPacket(const BlockHeader_cptr& lastBlockHeader);
    void onNewBlocks(const BlockHeader_cptr& lastBlockHeader);
    void onNewTransactionIds(const std::set<TransactionId>& transactionIds);
    void onTransactions(const std::map<TransactionId, Transaction_cptr>& transactionsMap);
    void onBlockHeader(const BlockHeader_cptr& blockHeader);
    void onCompletedBlock(const PendingBlock_ptr& pendingBlock);
    void onInvalidBlock(const PendingBlock_ptr& pendingBlock);
    void onExpiredBlock(const PendingBlock_ptr& pendingBlock, bool localExpiration);

    void sendFirstPendingTransactions();
    void sendNewTransactionIds(const std::set<TransactionId>& transactionIds);
    void requestNewTransactions();
    void requestBlockHeader();
    void requestBlock(const PendingBlock_ptr& pendingBlock);

    MinerId getMiner() const
    {
        return m_minerId;
    }

    json getDebugInfo() const;

private:
    const MinerId m_minerId;
    const std::shared_ptr<Blockchain> m_blockchain;
    const std::shared_ptr<const Database> m_database;
    Connection* m_connection = nullptr;

    mutable Logger m_logger;

    bool m_firstPacket = true;
    BlockHeader_cptr m_latestBlockHeader;
    SharedTransactionIds m_sharedTransactionIds{ 64, 2048 };

    SessionData m_data;
};

}
