#pragma once

#include "acceptor.h"
#include "blockchain/blockchain.h"
#include "communication_options.h"
#include "connection/connection.h"
#include "connection_manager.h"
#include "packets/packet.h"
#include "session.h"

namespace logpass {

class Communication;

// all protected functions are called on communication thread
class Communication : public EventLoopThread {
protected:
    friend class SharedThread<Communication>;

    Communication(const CommunicationOptions& options, const std::shared_ptr<Blockchain>& blockchain,
                  const std::shared_ptr<const Database>& database);
    void start() override;
    void stop() override;

public:

    CommunicationOptions getOptions() const
    {
        return m_options;
    }

    void getDebugInfo(SafeCallback<json>&& callback) const;

protected:
    // open acceptor, listen on specified host and port
    void close();
    virtual void check();
    void checkConnections();
    bool onConnection(Connection_ptr connection);
    Connection_ptr connect(const MinerId& minerId, const Endpoint& endpoint);

    virtual void onConnectionEnd(Connection* connection);
    virtual bool onConnectionParams(Connection* connection, const MinerId& minerId);
    virtual bool onConnectionReady(Connection* connection);
    virtual bool onConnectionPacket(Connection* connection, const Packet_ptr& packet);

    void onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch);
    void onNewTransactions(const std::vector<Transaction_cptr>& transactions);

    void updateMiners();

    Blockchain* getBlockchain() const
    {
        return m_blockchain.get();
    }

    const Database* getDatabase() const
    {
        return m_database.get();
    }

    const ConnectionManager& getConnectionManager() const
    {
        return m_connectionManager;
    }

protected:
    const CommunicationOptions m_options;
    const std::shared_ptr<Blockchain> m_blockchain;
    const std::shared_ptr<const Database> m_database;

    mutable Logger m_logger;

    asio::steady_timer m_timer;

    ConnectionManager m_connectionManager;
    std::unique_ptr<EventsListener> m_eventsListener;
    std::shared_ptr<Certificate> m_certificate;
    std::shared_ptr<Acceptor> m_acceptor;

    std::map<MinerId, std::shared_ptr<Session>> m_sessions;

    Hash m_firstBlocksHash;
    BlockHeader_cptr m_lastBlockHeader;
};

}
