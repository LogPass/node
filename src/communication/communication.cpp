#include "pch.h"

#include "communication.h"
#include "connection/secure_connection.h"
#include "connection/unsecure_connection.h"
#include "packets/packet.h"

namespace logpass {

Communication::Communication(const CommunicationOptions& options, const std::shared_ptr<Blockchain>& blockchain,
                             const std::shared_ptr<const Database>& database) :
    EventLoopThread("communication"), m_options(options), m_blockchain(blockchain), m_database(database),
    m_timer(m_context), m_connectionManager(blockchain->getMinerId())
{
    ASSERT(m_options.port != 0);

    m_logger.setClassName("Communication");
    m_logger.setId(m_options.host + ":"s + std::to_string(m_options.port));
}

void Communication::start()
{
    m_lastBlockHeader = m_database->confirmed().blocks.getLatestBlockHeader();
    m_firstBlocksHash = m_database->confirmed().blocks.getBlockHeader(1)->getHash();
    m_certificate = std::make_shared<Certificate>(m_blockchain->getMinerKey());
    m_connectionManager.setTrustedMiners(m_options.trustedNodes);

    post([this] {
        auto self(std::dynamic_pointer_cast<Communication>(shared_from_this()));
        ConnectionCallbacks callbacks{
            .onEnd = std::bind(&Communication::onConnectionEnd, self, std::placeholders::_1),
            .onParams = std::bind(&Communication::onConnectionParams, self, std::placeholders::_1, std::placeholders::_2),
            .onReady = std::bind(&Communication::onConnectionReady, self, std::placeholders::_1),
            .onPacket = std::bind(&Communication::onConnectionPacket, self, std::placeholders::_1, std::placeholders::_2),
        };

        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(m_options.host), m_options.port);
        m_acceptor = std::make_shared<Acceptor>(m_context, endpoint, m_blockchain->getMinerId(), m_certificate,
                                                std::bind(&Communication::onConnection, self, std::placeholders::_1),
                                                callbacks);
        m_acceptor->open();
        updateMiners();

        m_eventsListener = m_blockchain->registerEventsListener(EventsListenerCallbacks{
            .onBlocks = [this](auto blocks, bool didChangeBranch) {
                 post(std::bind(&Communication::onBlocks, this, blocks, didChangeBranch));
            },
            .onNewTransactions = [this](auto transactions) {
                post(std::bind(&Communication::onNewTransactions, this, transactions));
            }
        });

        check();
    });

    EventLoopThread::start();
}

void Communication::stop()
{
    post([this] {
        LOG_CLASS(debug) << "stopping";
        m_eventsListener = nullptr;
        close();
        m_acceptor = nullptr;
        m_timer.cancel();
    });
    EventLoopThread::stop();
    ASSERT(m_sessions.empty());
}

void Communication::close()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    m_acceptor->close();
    for (auto& connection : m_connectionManager.getConnections()) {
        connection->close("shutdown");
    }
}

void Communication::check()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    m_timer.expires_after(chrono::seconds(1));
    m_timer.async_wait([this](auto ec) { if (!ec && !isStopped()) {
        check();
    }});
    checkConnections();

    // check sessions
    for (auto& [minerId, session] : m_sessions) {
        try {
            session->onPeriodicalCheck();
        } catch (const SessionException& exception) {
            Connection* connection = m_connectionManager.getConnection(minerId);
            if (connection) {
                connection->close(exception.what());
                m_connectionManager.blockMinerId(connection->getMinerId(), std::chrono::seconds(60));
            }
        }
    }
}

void Communication::checkConnections()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());

    // close connections
    auto connectionsToEnd = m_connectionManager.getConnectionsToEnd();
    for (auto& connection : connectionsToEnd) {
        connection->close("requested by checkConnections");
    }

    // connect to miners
    auto connectionsCandidates = m_connectionManager.getConnectionCandidates();
    for (auto& [minerId, endpoint] : connectionsCandidates) {
        connect(minerId, endpoint);
    }
}

bool Communication::onConnection(Connection_ptr connection)
{
    if (m_connectionManager.getPendingConnections().size() >= kNetworkMaxPendingConnections) {
        return false;
    }

    if (!m_connectionManager.addConnection(connection.get())) {
        return false;
    }

    connection->start();
    return true;
}

Connection_ptr Communication::connect(const MinerId& minerId, const Endpoint& endpoint)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    if (isStopped()) {
        return nullptr;
    }
    if (!minerId.isValid() || minerId == m_blockchain->getMinerId()) {
        return nullptr;
    }
    if (m_connectionManager.hasPendingConnection(minerId)) {
        return nullptr;
    }
    if (!endpoint.isValid()) {
        return nullptr;
    }

    auto self(std::dynamic_pointer_cast<Communication>(shared_from_this()));
    ConnectionCallbacks callbacks{
        .onEnd = std::bind(&Communication::onConnectionEnd, self, std::placeholders::_1),
        .onParams = std::bind(&Communication::onConnectionParams, self, std::placeholders::_1, std::placeholders::_2),
        .onReady = std::bind(&Communication::onConnectionReady, self, std::placeholders::_1),
        .onPacket = std::bind(&Communication::onConnectionPacket, self, std::placeholders::_1, std::placeholders::_2),
    };

    Connection_ptr connection;
    if (!m_certificate) {
        connection = std::make_shared<UnsecureConnection>(m_context, std::move(asio::ip::tcp::socket(m_context)),
                                                          m_blockchain->getMinerId(), minerId, callbacks);
    } else {
        connection = std::make_shared<SecureConnection>(m_context, std::move(asio::ip::tcp::socket(m_context)),
                                                        m_certificate, m_blockchain->getMinerId(), minerId, callbacks);
    }

    if (!m_connectionManager.addConnection(connection.get())) {
        return nullptr;
    }

    connection->start(endpoint);
    return connection;
}

void Communication::onConnectionEnd(Connection* connection)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    if (connection->isAccepted()) {
        ASSERT(connection->getMinerId().isValid());
        auto it = m_sessions.find(connection->getMinerId());
        ASSERT(it != m_sessions.end());
        it->second->onDisconnected();
        m_sessions.erase(it);
    }
    m_connectionManager.removeConnection(connection);
}

bool Communication::onConnectionParams(Connection* connection, const MinerId& minerId)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    ASSERT(connection->getMinerId() == minerId);
    if (!minerId.isValid() || minerId == m_blockchain->getMinerId()) {
        return false;
    }
    bool isDesynchronized = m_blockchain->getLatestBlockId() + kMinersQueueSize < m_blockchain->getExpectedBlockId() ||
        m_blockchain->getLatestBlockId() == 1;
    if (!isDesynchronized && !m_database->confirmed().miners.getMiner(minerId) &&
        !m_options.trustedNodes.contains(minerId)) {
        return false; // miner does not exist
    }

    return m_connectionManager.canAcceptConnection(connection, isDesynchronized);
}

bool Communication::onConnectionReady(Connection* connection)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    ASSERT(connection->getMinerId().isValid());

    if (!m_connectionManager.acceptConnection(connection)) {
        return false;
    }

    ASSERT(m_sessions.count(connection->getMinerId()) == 0);
    auto session = std::make_shared<Session>(connection->getMinerId(), m_blockchain, m_database);
    session->onConnected(connection, m_lastBlockHeader);
    m_sessions.emplace(connection->getMinerId(), session);

    return true;
}

bool Communication::onConnectionPacket(Connection* connection, const Packet_ptr& packet)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    ASSERT(connection->isAccepted());
    auto sessionIt = m_sessions.find(connection->getMinerId());
    ASSERT(sessionIt != m_sessions.end());
    auto& session = sessionIt->second;
    try {
        return session->onPacket(packet);
    } catch (const SessionException& exception) {
        connection->close(exception.what());
        m_connectionManager.blockMinerId(connection->getMinerId(), std::chrono::seconds(60));
    }
    return false;
}


void Communication::onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    m_lastBlockHeader = blocks.back()->getBlockHeader();
    updateMiners();
    for (auto& [minerId, session] : m_sessions) {
        session->onBlocks(blocks, didChangeBranch);
    }
}

void Communication::onNewTransactions(const std::vector<Transaction_cptr>& transactions)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    for (auto& [minerId, session] : m_sessions) {
        session->onNewTransactions(transactions);
    }
}

void Communication::updateMiners()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());

    std::map<MinerId, Endpoint> highPriorityMiners;
    for (auto& nextMiner : m_database->confirmed().blocks.getMinersQueue()) {
        if (nextMiner == m_blockchain->getMinerId()) {
            continue;
        }
        if (highPriorityMiners.contains(nextMiner)) {
            continue;
        }
        Miner_cptr miner = m_database->confirmed().miners.getMiner(nextMiner);
        highPriorityMiners.emplace(miner->getId(), miner->settings.endpoint);
        if (highPriorityMiners.size() == kNetworkHighPriorityConnections) {
            break;
        }
    }

    std::map<MinerId, Endpoint> mediumPriorityMiners;
    for (auto& miner : m_database->confirmed().miners.getTopMiners()) {
        if (highPriorityMiners.contains(miner->id)) {
            continue;
        }
        mediumPriorityMiners.emplace(miner->getId(), miner->settings.endpoint);
    }

    std::map<MinerId, Endpoint> lowPriorityMiners = m_database->confirmed().miners.getMinerEndpoints();

    highPriorityMiners.erase(m_blockchain->getMinerId());
    mediumPriorityMiners.erase(m_blockchain->getMinerId());
    lowPriorityMiners.erase(m_blockchain->getMinerId());

    m_connectionManager.setPriorityMiners(highPriorityMiners, mediumPriorityMiners, lowPriorityMiners);
}

void Communication::getDebugInfo(SafeCallback<json>&& callback) const
{
    post([this, callback = std::move(callback)]{
        json sessions = std::map<std::string, json>();
        for (auto& [minerId, session] : m_sessions) {
            sessions.emplace(minerId.toString(), session->getDebugInfo());
        }

        callback(json{
            { "connections", m_connectionManager.getDebugInfo() },
            { "sessions", sessions }
        });
    });
}

}
