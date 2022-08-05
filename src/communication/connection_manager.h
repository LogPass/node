#pragma once

#include "connection/connection.h"
#include <database/database.h>

namespace logpass {

// class is not thread-safe
class ConnectionManager {
public:
    ConnectionManager(const MinerId& minerId);
    ~ConnectionManager();
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    Connection* getConnection(const MinerId& minerId) const;
    std::set<Connection*> getConnections() const;
    std::set<Connection*> getActiveConnections() const;
    std::set<Connection*> getPendingConnections() const;

    bool hasPendingConnection(const MinerId& minerId) const;

    bool addConnection(Connection* connection);
    bool canAcceptConnection(Connection* connection, bool isDesynchronized = false);
    bool acceptConnection(Connection* connection);
    void removeConnection(Connection* connection);

    // returns map of miner ids with endpoint to connect to
    std::map<MinerId, Endpoint> getConnectionCandidates();

    // returns set of connections to close
    std::set<Connection*> getConnectionsToEnd();

    void setTrustedMiners(const std::map<MinerId, Endpoint>& trustedMiners);
    void setPriorityMiners(const std::map<MinerId, Endpoint>& highPriorityMiners,
                           const std::map<MinerId, Endpoint>& mediumPriorityMiners,
                           const std::map<MinerId, Endpoint>& lowPriorityMiners);

    // temporary blocks connections to miner with given id
    void blockMinerId(const MinerId& minerId, const std::chrono::seconds& duration = std::chrono::seconds(15));

    void clearBlockedMinerIds();

    void recalculateConnectionsCount();

    json getDebugInfo() const;

    void sortMiners(std::vector<std::pair<MinerId, Endpoint>>& miners);

    size_t getMinerIndex(const MinerId& minerId) const;

    // pseudo random, deterministic random number generator to determine connection order
    uint64_t calculateMinerPriority(uint64_t x) const
    {
        x ^= m_randomValue;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        return x;
    }

protected:
    const MinerId m_minerId;
    uint64_t m_randomValue = 0;

    boost::bimap<Connection*, MinerId> m_connections;
    std::set<Connection*> m_pendingConnections;
    std::set<MinerId> m_pendingConnectionsMinerIds;

    std::array<std::vector<std::pair<MinerId, Endpoint>>, 4> m_miners;
    std::array<std::pair<size_t, size_t>, 4> m_limits = {}; // incoming, outgoing
    std::array<std::pair<size_t, size_t>, 4> m_connectionsCount = {}; // incoming, outgoing

    std::map<MinerId, chrono::steady_clock::time_point> m_blockedIncomingConnections;
    std::map<MinerId, chrono::steady_clock::time_point> m_blockedOutgoingConnections;
};

}
