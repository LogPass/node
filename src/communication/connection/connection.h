#pragma once

#include "connection_callbacks.h"
#include <communication/packets/packet.h>

namespace logpass {

class Communication;
class Connection;
using Connection_ptr = std::shared_ptr<Connection>;
using Connection_wptr = std::weak_ptr<Connection>;

// not thread-safe
class Connection : public std::enable_shared_from_this<Connection> {
public:
    enum class Phase : uint8_t {
        WAITING_FOR_START,
        WAITING_FOR_CONNECTION,
        WAITING_FOR_FIRST_PACKET,
        WORKING
    };

    Connection(asio::io_context& context, const MinerId& localMinerId, const MinerId& remoteMinerId,
               const ConnectionCallbacks& callbacks);
    virtual ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // starts incoming connection
    virtual void start() = 0;
    // starts outgoing connection
    virtual void start(const Endpoint& endpoint) = 0;
    // closes connection
    virtual void close(std::string_view reason = "");
    // sends packet
    void send(const Packet_ptr& packet);

    virtual uint32_t getMaxPacketSize() const
    {
        if (m_phase == Phase::WORKING) {
            return kNetworkMaxPacketSize;
        }
        return 1024;
    }

    virtual size_t getTimeout() const
    {
        return kNetworkConnectionTimeout * 1000;
    }

    chrono::steady_clock::time_point getLastKeepAliveTimePoint() const
    {
        return m_lastKeepAlive;
    }

    chrono::steady_clock::time_point getStartTime() const
    {
        return m_startTime;
    }

    bool isClosed() const
    {
        return m_closed;
    }

    bool isOutgoing() const
    {
        return m_outgoing;
    }

    bool isAccepted() const
    {
        return m_phase == Phase::WORKING;
    }

    MinerId getMinerId() const
    {
        return m_remoteMinerId;
    }

    json getDebugInfo() const;

protected:
    virtual void read() = 0;
    virtual void write(const Serializer_cptr& msg) = 0;

    void onConnected();
    void onRead(const Serializer_ptr& msg);
    void onWrite();
    void onTimeout(const boost::system::error_code& ec);

private:
    void onConnectionReady();

    void processRawPacket(const Serializer_ptr& s);
    void send(const Serializer_cptr& s);
    void keepAlive(const boost::system::error_code& ec);

    void sendFirstPacket();
    void onFirstPacket(const Serializer_ptr& s);

    void onKeepAlivePacket();
    void onPacket(const Packet_ptr& packet);

protected:
    asio::io_context& m_context;
    const ConnectionCallbacks m_callbacks;

    asio::steady_timer m_readTimer;
    asio::steady_timer m_writeTimer;
    asio::steady_timer m_keepAliveTimer;

    mutable Logger m_logger;

    bool m_closed = false;
    bool m_validated = false;
    const bool m_outgoing = false;
    Phase m_phase = Phase::WAITING_FOR_START;

    uint32_t m_packetId = 1, m_expectedPacketId = 1;
    size_t m_bytesSent = 0, m_bytesRecived = 0;
    std::queue<Serializer_cptr> m_sendQueue;
    std::map<uint32_t, std::pair<Packet_ptr, chrono::steady_clock::time_point>> m_waitingPackets;

    // packets waiting to be send when connection is ready
    std::queue<Packet_ptr> m_queuedPackets;

    const MinerId m_localMinerId;
    MinerId m_remoteMinerId;
    chrono::steady_clock::time_point m_startTime;
    chrono::steady_clock::time_point m_lastKeepAlive;
};

}
