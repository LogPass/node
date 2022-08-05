#include "pch.h"

#include "connection_manager.h"

namespace logpass {

ConnectionManager::ConnectionManager(const MinerId& minerId) : m_minerId(minerId)
{
    ASSERT(m_minerId.isValid());
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    m_randomValue = std::uniform_int_distribution<uint64_t>()(generator);
    m_limits[0] = { kNetworkHighPriorityConnections, kNetworkHighPriorityConnections };
    m_limits[1] = { kNetworkHighPriorityConnections, kNetworkHighPriorityConnections };
    m_limits[2] = { kNetworkMediumPriorityIncomingConnections, kNetworkMediumPriorityOutgoingConnections };
    m_limits[3] = { kNetworkLowPriorityIncomingConnections, kNetworkLowPriorityOutgoingConnections };
}

ConnectionManager::~ConnectionManager()
{
    ASSERT(m_connections.empty());
    ASSERT(m_pendingConnections.empty());
    ASSERT(m_pendingConnectionsMinerIds.empty());
}

Connection* ConnectionManager::getConnection(const MinerId& minerId) const
{
    auto it = m_connections.right.find(minerId);
    if (it == m_connections.right.end()) {
        return nullptr;
    }
    return it->second;
}

std::set<Connection*> ConnectionManager::getConnections() const
{
    std::set<Connection*> connections = m_pendingConnections;
    for (auto& [connection, minerId] : m_connections) {
        connections.insert(connection);
    }
    return connections;
}

std::set<Connection*> ConnectionManager::getActiveConnections() const
{
    std::set<Connection*> connections;
    for (auto& [connection, minerId] : m_connections) {
        connections.insert(connection);
    }
    return connections;
}

std::set<Connection*> ConnectionManager::getPendingConnections() const
{
    return m_pendingConnections;
}

bool ConnectionManager::hasPendingConnection(const MinerId& minerId) const
{
    return m_pendingConnectionsMinerIds.contains(minerId);
}

bool ConnectionManager::addConnection(Connection* connection)
{
    ASSERT(m_connections.left.count(connection) == 0);
    ASSERT(!m_pendingConnections.contains(connection));
    ASSERT(!connection->isAccepted());

    if (!connection->isOutgoing()) {
        if (m_pendingConnections.size() > kNetworkMaxPendingConnections + m_pendingConnectionsMinerIds.size()) {
            return false;
        }
    } else {
        if (m_pendingConnectionsMinerIds.size() > kNetworkMaxPendingConnections) {
            return false;
        }
    }

    m_pendingConnections.insert(connection);
    if (connection->isOutgoing()) {
        ASSERT(connection->getMinerId().isValid());
        m_pendingConnectionsMinerIds.insert(connection->getMinerId());
    }
    return true;
}

bool ConnectionManager::canAcceptConnection(Connection* connection, bool isDesynchronized)
{
    ASSERT(!connection->isAccepted());
    ASSERT(connection->getMinerId().isValid());
    ASSERT(m_connections.left.count(connection) == 0);

    if (!m_pendingConnections.contains(connection)) {
        return false;
    }

    auto it = m_connections.right.find(connection->getMinerId());
    if (it != m_connections.right.end()) {
        return false;
    }

    // if it's incoming connection and there's a pending connection to this miner and remote minerId is greater 
    // than our minerId then don't allow connection, let us connect to this miner
    if (!connection->isOutgoing() && m_pendingConnectionsMinerIds.contains(connection->getMinerId()) &&
        connection->getMinerId() > m_minerId) {
        return false;
    }

    if (!isDesynchronized) {
        size_t minerIndex = getMinerIndex(connection->getMinerId());
        if (minerIndex == m_miners.size()) {
            return false;
        }
    }

    if (!connection->isOutgoing()) {
        auto it = m_blockedIncomingConnections.find(connection->getMinerId());
        if (it != m_blockedIncomingConnections.end()) {
            if (it->second > std::chrono::steady_clock::now()) {
                return false;
            }
            m_blockedIncomingConnections.erase(it);
        }
    }

    return true;
}

bool ConnectionManager::acceptConnection(Connection* connection)
{
    ASSERT(!connection->isAccepted());
    ASSERT(m_connections.left.count(connection) == 0);
    ASSERT(m_pendingConnections.contains(connection));

    if (m_connections.right.count(connection->getMinerId()) != 0) {
        return false;
    }

    size_t minerIndex = getMinerIndex(connection->getMinerId());
    if (minerIndex == m_miners.size()) {
        minerIndex = m_miners.size() - 1;
    }

    if (connection->isOutgoing()) {
        if (m_connectionsCount[minerIndex].second > m_limits[minerIndex].second) {
            return false;
        }
        m_connectionsCount[minerIndex].second += 1;
    } else {
        if (m_connectionsCount[minerIndex].first > m_limits[minerIndex].first) {
            return false;
        }
        m_connectionsCount[minerIndex].first += 1;
    }

    m_connections.insert({ connection, connection->getMinerId() });
    m_pendingConnections.erase(connection);
    if (connection->isOutgoing()) {
        m_pendingConnectionsMinerIds.erase(connection->getMinerId());
    }

    return true;
}

void ConnectionManager::removeConnection(Connection* connection)
{
    if (m_pendingConnections.contains(connection)) {
        m_pendingConnections.erase(connection);
        if (connection->isOutgoing() && !connection->isAccepted()) {
            m_pendingConnectionsMinerIds.erase(connection->getMinerId());
        }
    }

    auto it = m_connections.left.find(connection);
    if (it != m_connections.left.end()) {
        m_connections.left.erase(it);

        size_t minerIndex = getMinerIndex(connection->getMinerId());
        if (minerIndex == m_miners.size()) {
            minerIndex = m_miners.size() - 1;
        }

        if (connection->isOutgoing()) {
            m_connectionsCount[minerIndex].second -= 1;
        } else {
            m_connectionsCount[minerIndex].first -= 1;
        }
    }

    // block miner id if connection was short lived
    if (connection->getStartTime() + chrono::seconds(60) > chrono::steady_clock::now()) {
        if (connection->isOutgoing()) {
            m_blockedOutgoingConnections.emplace(connection->getMinerId(),
                chrono::steady_clock::now() + chrono::seconds(30));
        } else if (connection->getMinerId().isValid()) {
            m_blockedIncomingConnections.emplace(connection->getMinerId(),
                chrono::steady_clock::now() + chrono::seconds(30));
        }
    }
}

std::map<MinerId, Endpoint> ConnectionManager::getConnectionCandidates()
{
    std::map<MinerId, Endpoint> selectedCandidates;
    auto now = std::chrono::steady_clock::now();
    for (size_t i = 0; i < m_miners.size(); ++i) {
        size_t count = m_connectionsCount[i].second;
        for (auto& [minerId, endpoint] : m_miners[i]) {
            if (count > m_limits[i].first) {
                break;
            }
            if (!endpoint.isValid()) {
                continue;
            }
            if (getConnection(minerId)) {
                continue;
            }
            if (hasPendingConnection(minerId)) {
                continue;
            }
            if (selectedCandidates.contains(minerId)) {
                continue;
            }
            auto it = m_blockedOutgoingConnections.find(minerId);
            if (it != m_blockedOutgoingConnections.end()) {
                if (it->second > now) {
                    continue;
                }
                m_blockedOutgoingConnections.erase(it);
            }
            selectedCandidates.emplace(minerId, endpoint);
            count += 1;
        }
    }

    return selectedCandidates;
}

std::set<Connection*> ConnectionManager::getConnectionsToEnd()
{
    std::set<Connection*> connectionsToEnd;
    for (size_t i = 0; i < m_miners.size(); ++i) {
        std::pair<size_t, size_t> count = m_connectionsCount[i];
        for (auto& [minerId, endpoint] : m_miners[i] | views::reverse) {
            if (count.first <= m_limits[i].first && count.second <= m_limits[i].second) {
                break;
            }
            Connection* connection = getConnection(minerId);
            if (!connection) {
                continue;
            }
            if (connection->isOutgoing()) {
                if (count.second > m_limits[i].second) {
                    connectionsToEnd.insert(connection);
                    count.second -= 1;
                }
            } else {
                if (count.first > m_limits[i].first) {
                    connectionsToEnd.emplace(connection);
                    count.first -= 1;
                }
            }
        }
    }
    return connectionsToEnd;
}

void ConnectionManager::clearBlockedMinerIds()
{
    m_blockedIncomingConnections.clear();
    m_blockedOutgoingConnections.clear();
}

void ConnectionManager::setTrustedMiners(const std::map<MinerId, Endpoint>& trustedMiners)
{
    std::transform(trustedMiners.begin(), trustedMiners.end(), std::back_inserter(m_miners[0]),
        [](auto& kv) { return kv; });
    sortMiners(m_miners[0]);
    recalculateConnectionsCount();
}

void ConnectionManager::setPriorityMiners(const std::map<MinerId, Endpoint>& highPriorityMiners,
    const std::map<MinerId, Endpoint>& mediumPriorityMiners,
    const std::map<MinerId, Endpoint>& lowPriorityMiners)
{
    m_miners[1].clear();
    m_miners[2].clear();
    m_miners[3].clear();

    m_miners[1].reserve(highPriorityMiners.size());
    m_miners[2].reserve(mediumPriorityMiners.size());
    m_miners[3].reserve(lowPriorityMiners.size());

    std::transform(highPriorityMiners.begin(), highPriorityMiners.end(), std::back_inserter(m_miners[1]),
        [](auto& kv) { return kv; });
    std::transform(mediumPriorityMiners.begin(), mediumPriorityMiners.end(), std::back_inserter(m_miners[2]),
        [](auto& kv) { return kv; });
    std::transform(lowPriorityMiners.begin(), lowPriorityMiners.end(), std::back_inserter(m_miners[3]),
        [](auto& kv) { return kv; });

    sortMiners(m_miners[1]);
    sortMiners(m_miners[2]);
    sortMiners(m_miners[3]);
    recalculateConnectionsCount();
}

void ConnectionManager::blockMinerId(const MinerId& minerId, const std::chrono::seconds& duration)
{
    auto it1 = m_blockedIncomingConnections.emplace(minerId, std::chrono::steady_clock::now()).first;
    auto it2 = m_blockedOutgoingConnections.emplace(minerId, std::chrono::steady_clock::now()).first;
    auto timePoint = std::max(std::max(it1->second, it2->second), std::chrono::steady_clock::now() + duration);
    it1->second = timePoint;
    it2->second = timePoint;
}

void ConnectionManager::recalculateConnectionsCount()
{
    m_connectionsCount = {};
    for (auto& [connection, minerId] : m_connections) {
        size_t minerIndex = getMinerIndex(connection->getMinerId());
        if (minerIndex == m_miners.size()) {
            minerIndex = m_miners.size() - 1;
        }

        if (connection->isOutgoing()) {
            m_connectionsCount[minerIndex].second += 1;
        } else {
            m_connectionsCount[minerIndex].first += 1;
        }
    }
}

void ConnectionManager::sortMiners(std::vector<std::pair<MinerId, Endpoint>>& miners)
{
    std::sort(miners.begin(), miners.end(), [&](const auto& a, const auto& b) {
        uint64_t valA = calculateMinerPriority(*reinterpret_cast<const uint64_t*>(a.first.data()));
        uint64_t valB = calculateMinerPriority(*reinterpret_cast<const uint64_t*>(b.first.data()));
        if (valA == valB) {
            return a.first < b.first;
        }
        return valA < valB;
        });
}

size_t ConnectionManager::getMinerIndex(const MinerId& minerId) const
{
    for (size_t i = 0; i < m_miners.size(); ++i) {
        bool exists = std::binary_search(m_miners[i].begin(), m_miners[i].end(), std::make_pair(minerId, Endpoint()),
            [&](const auto& a, const auto& b) {
                uint64_t valA = calculateMinerPriority(*reinterpret_cast<const uint64_t*>(a.first.data()));
                uint64_t valB = calculateMinerPriority(*reinterpret_cast<const uint64_t*>(b.first.data()));
                if (valA == valB) {
                    return a.first < b.first;
                }
                return valA < valB;
            });
        if (exists) {
            return i;
        }
    }
    return m_miners.size();
}

json ConnectionManager::getDebugInfo() const
{
    json j = {
        {"pending_connections", std::vector<json>()},
        {"connections", std::map<std::string, json>()},
        {"blocked_outgoing_connections", std::map<std::string, size_t>()},
        {"blocked_incoming_connections", std::map<std::string, size_t>()},
    };
    for (auto& it : m_pendingConnections) {
        j["pending_connections"].push_back(it->getDebugInfo());
    }
    for (auto& it : m_connections) {
        j["connections"].emplace(it.right.toString(), it.left->getDebugInfo());
    }
    for (size_t i = 0; i < m_connectionsCount.size(); ++i) {
        j["connections_count"].push_back(json{
            {"in", m_connectionsCount[i].first},
            {"out", m_connectionsCount[i].second},
        });
    }
    j["miners"] = m_miners;
    auto now = std::chrono::steady_clock::now();
    for (auto& [minerId, blockade] : m_blockedOutgoingConnections) {
        if (blockade <= now) {
            continue;
        }
        size_t time = std::chrono::duration_cast<chrono::seconds>(blockade - now).count();
        j["blocked_outgoing_connections"].emplace(minerId.toString(), time);
    }
    for (auto& [minerId, blockade] : m_blockedIncomingConnections) {
        if (blockade <= now) {
            continue;
        }
        size_t time = std::chrono::duration_cast<chrono::seconds>(blockade - now).count();
        j["blocked_incoming_connections"].emplace(minerId.toString(), time);
    }
    return j;
}

}
