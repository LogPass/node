#pragma once

#include "communication.h"

namespace logpass {

// Class for testing communication
class CommunicationTest : public Communication {
protected:
    friend class SharedThread<CommunicationTest>;
    CommunicationTest(const CommunicationOptions& options, const std::shared_ptr<Blockchain>& blockchain,
                      const std::shared_ptr<const Database>& database) :
        Communication(options, blockchain, database)
    {};

public:
    void check() override {}

    // do single check
    void check(bool wait)
    {
        post([&] {
            Communication::check();
            m_timer.cancel();
        }, wait);
    }

    void checkConnections()
    {
        post([&] {
            m_connectionManager.clearBlockedMinerIds();
            Communication::checkConnections();
        }, true);
    }

    void clearBlockMinerIds()
    {
        post([&] {
            m_connectionManager.clearBlockedMinerIds();
        }, true);
    }

    Connection_ptr connect(const Miner_cptr& miner)
    {
        ASSERT(!m_promise);
        m_promise = std::make_shared<std::promise<Connection_ptr>>();
        asio::post(m_context, [this, miner] {
            m_connection = Communication::connect(miner->id, miner->settings.endpoint);
        });
        auto f = m_promise->get_future();
        Connection_ptr ret = nullptr;
        if (f.wait_for(chrono::seconds(3)) == std::future_status::ready) {
            ret = f.get();
        }
        m_promise = nullptr;
        return ret;
    }

    Connection_ptr connect(const MinerId& minerId, const Endpoint& endpoint)
    {
        auto miner = Miner::create(minerId, UserId(), 1);
        miner->settings.endpoint = endpoint;
        return connect(miner);
    }

    void onConnectionEnd(Connection* connection) override
    {
        if (connection == m_connection.get() && m_promise) {
            m_promise->set_value(nullptr);
            m_connection = nullptr;
        }
        Communication::onConnectionEnd(connection);
    }

    bool onConnectionParams(Connection* connection, const MinerId& miner) override
    {
        return Communication::onConnectionParams(connection, miner);
    }

    bool onConnectionReady(Connection* connection) override
    {
        if (!Communication::onConnectionReady(connection))
            return false;
        if (connection == m_connection.get() && m_promise) {
            m_promise->set_value(m_connection);
            m_connection = nullptr;
        }
        return true;
    }

    bool onConnectionPacket(Connection* connection, const Packet_ptr& packet) override
    {
        m_lastPacket = packet;
        return Communication::onConnectionPacket(connection, packet);
    }

    Connection* getConnectionByMinerId(const MinerId& minerId)
    {
        std::promise<Connection*> promise;
        post([&] {
            promise.set_value(m_connectionManager.getConnection(minerId));
        });
        return promise.get_future().get();
    }

    std::shared_ptr<Session> getSession(const MinerId& minerId)
    {
        std::promise<std::shared_ptr<Session>> promise;
        post([&] {
            auto it = m_sessions.find(minerId);
            if (it == m_sessions.end()) {
                promise.set_value(nullptr);
                return;
            }
            promise.set_value(it->second);
        });
        return promise.get_future().get();
    }

    size_t getConnectionsCount()
    {
        std::promise<size_t> promise;
        post([&] {
            promise.set_value(m_connectionManager.getConnections().size());
        });
        return promise.get_future().get();
    }

    size_t getActiveConnectionsCount()
    {
        std::promise<size_t> promise;
        post([&] {
            promise.set_value(m_connectionManager.getActiveConnections().size());
        });
        return promise.get_future().get();
    }

    void closeAllConnections()
    {
        post([&] {
            for (auto& connection : m_connectionManager.getConnections()) {
                connection->close("requested by test");
            }
        }, true);
    }

    std::string getDebugInfo()
    {
        std::promise<std::string> promise;
        Communication::getDebugInfo((SafeCallback<json>)[&](auto json) {
            json->emplace("id", m_blockchain->getMinerId());
            promise.set_value(json->dump(4));
        });
        return promise.get_future().get();
    }

    Packet_ptr getLastPacket()
    {
        return m_lastPacket;
    }

    uint16_t getPort()
    {
        return m_options.port;
    }

    void post(std::function<void(void)>&& task, bool wait = false)
    {
        if (wait) {
            std::promise<bool> promise;
            post([&] {
                task();
                promise.set_value(true);
            }, false);
            promise.get_future().get();
            return;
        }
        Communication::post(std::move(task));
    }

    Connection_ptr m_connection;
    Packet_ptr m_lastPacket;
    std::shared_ptr<std::promise<Connection_ptr>> m_promise;
};

}
