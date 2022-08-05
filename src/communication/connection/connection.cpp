#include "pch.h"

#include "connection.h"

namespace logpass {

Connection::Connection(asio::io_context& context, const MinerId& localMinerId, const MinerId& remoteMinerId,
                       const ConnectionCallbacks& callbacks) :
    m_context(context), m_localMinerId(localMinerId), m_remoteMinerId(remoteMinerId), m_callbacks(callbacks),
    m_outgoing(remoteMinerId.isValid()), m_readTimer(context), m_writeTimer(context), m_keepAliveTimer(context),
    m_lastKeepAlive(chrono::steady_clock::now()), m_startTime(chrono::steady_clock::now())
{
    m_logger.add_attribute("Class", boost::log::attributes::constant<std::string>("Connection"));
}

Connection::~Connection()
{
    ASSERT(m_closed);
}

void Connection::close(std::string_view reason)
{
    if (m_closed) {
        return;
    }

    LOG_CLASS(debug) << "close, reason: " << reason;

    m_closed = true;

    boost::system::error_code ec;
    m_readTimer.cancel(ec);
    m_writeTimer.cancel(ec);
    m_keepAliveTimer.cancel(ec);

    if (m_phase == Phase::WAITING_FOR_START) {
        return; // connect didn't start, don't call onEnd
    }

    asio::post(m_context, [this, self = shared_from_this()]() {
        if (m_callbacks.onEnd) {
            m_callbacks.onEnd(this);
        }
    });
}

void Connection::onConnected()
{
    if (m_closed) {
        return;
    }

    ASSERT(m_phase == Phase::WAITING_FOR_CONNECTION);

    LOG_CLASS(trace) << "connected";

    m_phase = Phase::WAITING_FOR_FIRST_PACKET;
    m_startTime = chrono::steady_clock::now();

    if (m_outgoing) {
        sendFirstPacket();
    }

    m_readTimer.expires_after(chrono::milliseconds(getTimeout()));
    m_readTimer.async_wait(std::bind(&Connection::onTimeout, shared_from_this(), std::placeholders::_1));
    read();
}


void Connection::onConnectionReady()
{
    ASSERT(m_phase == Phase::WAITING_FOR_FIRST_PACKET);
    ASSERT(m_localMinerId.isValid() && m_remoteMinerId.isValid());
    if (m_closed) {
        return;
    }

    if (m_callbacks.onReady && !m_callbacks.onReady(this)) {
        return close("onConnectionReady failed");
    }

    LOG_CLASS(trace) << "connection is ready";

    if (!m_outgoing) {
        sendFirstPacket();
    }

    m_validated = true;
    m_phase = Phase::WORKING;

    m_lastKeepAlive = chrono::steady_clock::now();
    m_keepAliveTimer.expires_after(chrono::milliseconds(getTimeout() / 2));
    m_keepAliveTimer.async_wait(std::bind(&Connection::keepAlive, shared_from_this(), std::placeholders::_1));

    while (!m_queuedPackets.empty()) {
        Packet_ptr packet = m_queuedPackets.front();
        LOG_CLASS(trace) << "sending queued packet (" << (uint16_t)packet->getType() << ")";
        send(packet);
        m_queuedPackets.pop();
    }
}

void Connection::onRead(const Serializer_ptr& msg)
{
    ASSERT(msg->reader());
    if (m_closed) {
        return;
    }

    // statistics
    m_bytesRecived += msg->size();

    // close connection if it waits too long for reply packet
    if (!m_waitingPackets.empty() && chrono::steady_clock::now() >
        m_waitingPackets.begin()->second.second + chrono::milliseconds(getTimeout())) {
        return close("waited too long for response packet");
    }

    if (msg->size() == 0) {
        if (m_phase == Phase::WORKING) {
            onKeepAlivePacket();
            m_readTimer.expires_after(chrono::milliseconds(getTimeout()));
            m_readTimer.async_wait(std::bind(&Connection::onTimeout, shared_from_this(), std::placeholders::_1));
            return read();
        }
        return close("invalid header size (0)");
    }

    if (msg->size() > getMaxPacketSize()) {
        return close("invalid header size ("s + std::to_string(msg->size()) + ")");
    }

    processRawPacket(msg);

    if (m_closed) {
        return;
    }

    m_readTimer.expires_after(chrono::milliseconds(getTimeout()));
    m_readTimer.async_wait(std::bind(&Connection::onTimeout, shared_from_this(), std::placeholders::_1));
    read();
}

void Connection::processRawPacket(const Serializer_ptr& msg)
{
    try {
        if (m_phase == Phase::WAITING_FOR_FIRST_PACKET) {
            onFirstPacket(msg);
        } else {
            uint32_t packetId = msg->get<uint32_t>();
            if (packetId != m_expectedPacketId) {
                return close("invalid packetId");
            }
            m_expectedPacketId += 1;
            if (msg->peek<uint8_t>() != 0x00) {
                auto packet = Packet::load(*msg, packetId);
                LOG_CLASS(trace) << "received packet (" << (uint16_t)packet->getType() << "), id: " << packet->getId();
                onPacket(packet);
            } else {
                // it's a reply for some previous packet
                msg->get<uint8_t>(); // skip 1 byte
                uint32_t replyForPacketId = msg->get<uint32_t>();
                auto it = m_waitingPackets.find(replyForPacketId);
                if (it == m_waitingPackets.end())
                    return close("invalid response packet id");
                Packet_ptr packet = it->second.first;
                m_waitingPackets.erase(it);
                packet->serializeResponse(*msg);
                LOG_CLASS(trace) << "received reply packet (" << (uint16_t)packet->getType() << "), id: " <<
                    replyForPacketId;
                onPacket(packet);
            }
        }
    } catch (SerializerException& e) {
        LOG_CLASS(warning) << "packet parsing error: " << e.what();
        return close("invalid packet");
    }
}

void Connection::send(const Packet_ptr& packet)
{
    ASSERT(packet);
    if (m_closed) {
        return;
    }

    if (m_phase != Phase::WORKING) {
        // connection is not ready, add packet to queue and send it when connection is ready
        ASSERT(packet->getId() == 0);
        LOG_CLASS(trace) << "adding packet (" << (uint16_t)packet->getType() << ") to queued packets";
        m_queuedPackets.push(packet);
        return;
    }

    auto s = std::make_shared<Serializer>();
    s->serialize(m_packetId);
    if (packet->hasResponse()) {
        ASSERT(packet->requiresResponse());
        ASSERT(packet->getId() > 0);
        s->put<uint8_t>(0);
        s->put<uint32_t>(packet->getId());
        packet->serializeResponse(*s);
    } else {
        ASSERT(packet->getId() == 0);
        packet->serializeRequest(*s);
        if (packet->requiresResponse()) {
            if (m_waitingPackets.size() >= 64) { // limits waiting packets size to 64
                return close("too many waiting packets");
            }
            m_waitingPackets.emplace(m_packetId, std::make_pair(packet, chrono::steady_clock::now()));
        }
    }

    m_packetId += 1;
    LOG_CLASS(trace) << "sending packet (" << (uint16_t)packet->getType() << "), id: " << packet->getId();
    send(s);
}

void Connection::send(const Serializer_cptr& msg)
{
    ASSERT(msg->writer());
    ASSERT(msg->size() <= getMaxPacketSize());

    if (m_closed) {
        return;
    }

    if (msg->reader()) {
        return;
    }

    if (m_sendQueue.empty()) {
        m_sendQueue.push(msg);
    }

    if (m_sendQueue.front() != msg) {
        if (m_sendQueue.size() >= 64) { // limits send buffer size to 64 messages
            return close("too many waiting packets");
        }
        m_sendQueue.push(msg);
        return;
    }

    // statistics
    m_bytesSent += msg->size();

    m_writeTimer.expires_after(chrono::milliseconds(getTimeout()));
    m_writeTimer.async_wait(std::bind(&Connection::onTimeout, shared_from_this(), std::placeholders::_1));

    write(msg);
}

void Connection::onWrite()
{
    if (m_closed) {
        return;
    }

    m_sendQueue.pop();
    if (m_sendQueue.empty()) {
        m_writeTimer.cancel();
        return;
    }

    send(m_sendQueue.front());
}

void Connection::keepAlive(const boost::system::error_code& ec)
{
    if (m_closed) {
        return;
    }

    if (ec) {
        return;
    }

    m_keepAliveTimer.expires_after(chrono::milliseconds(getTimeout() / 2));
    m_keepAliveTimer.async_wait(std::bind(&Connection::keepAlive, shared_from_this(), std::placeholders::_1));

    send(std::make_shared<Serializer>());
}

void Connection::sendFirstPacket()
{
    ASSERT(m_localMinerId.isValid() && m_remoteMinerId.isValid());
    auto s = std::make_shared<Serializer>();
    (*s)(kNetworkProtocolVersion);
    (*s)(m_localMinerId);
    (*s)(m_remoteMinerId);
    send(s);
}

void Connection::onFirstPacket(const Serializer_ptr& s)
{
    uint8_t protocolVersion = s->get<uint8_t>();
    if (protocolVersion != kNetworkProtocolVersion) {
        return close("invalid protocol version");
    }
    MinerId localMinerId, remoteMinerId;
    (*s)(remoteMinerId);
    (*s)(localMinerId);
    if (!s->eof()) {
        return close("invalid first packet data");
    }

    if (m_localMinerId != localMinerId) {
        return close("invalid local miner id");
    }

    if (!m_remoteMinerId.isValid()) {
        m_remoteMinerId = remoteMinerId;
        m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(m_remoteMinerId.toString()));
        if (m_callbacks.onParams && !m_callbacks.onParams(this, m_remoteMinerId)) {
            return close("onConnectionParams failed");
        }
    } else if (m_remoteMinerId != remoteMinerId) {
        return close("invalid remote miner id");
    }

    onConnectionReady();
}

void Connection::onTimeout(const boost::system::error_code& ec)
{
    if (ec) {
        return;
    }
    close("timeout");
}

void Connection::onKeepAlivePacket()
{
    ASSERT(!m_closed);
    auto now = chrono::steady_clock::now();
    auto difference = now - m_lastKeepAlive;
    if (difference <= chrono::milliseconds(getTimeout() / 4)) {
        return close("invalid keep alive packet");
    }
    m_lastKeepAlive = now;
}

void Connection::onPacket(const Packet_ptr& packet)
{
    ASSERT(!m_closed);
    if (!m_callbacks.onPacket) {
        return;
    }
    if (!m_callbacks.onPacket(this, packet)) {
        return close("onPacket failed");
    }
}

json Connection::getDebugInfo() const
{
    auto now = chrono::steady_clock::now();
    return {
        {"miner_id", m_remoteMinerId},
        {"outgoing", m_outgoing},
        {"validated", m_validated},
        {"packet_id", m_packetId},
        {"expected_packet_id", m_expectedPacketId},
        {"bytes_sent", m_bytesSent},
        {"bytes_recived", m_bytesRecived},
        {"sent_queue_size", m_sendQueue.size()},
        {"waiting_packets_size", m_waitingPackets.size()},
        {"last_keep_alive", chrono::duration_cast<chrono::seconds>(now - m_lastKeepAlive).count()},
        {"duration", chrono::duration_cast<chrono::seconds>(now - m_startTime).count()}
    };
}


}
