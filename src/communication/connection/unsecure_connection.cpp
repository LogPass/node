#include "pch.h"

#include "unsecure_connection.h"

namespace logpass {

UnsecureConnection::UnsecureConnection(asio::io_context& context, asio::ip::tcp::socket&& socket,
                                       const MinerId& localMinerId, const MinerId& remoteMinerId,
                                       const ConnectionCallbacks& callbacks) :
    Connection(context, localMinerId, remoteMinerId, callbacks), m_socket(std::move(socket)), m_resolver(context)
{}

void UnsecureConnection::start()
{
    ASSERT(!m_closed);
    ASSERT((!m_outgoing && m_phase == Phase::WAITING_FOR_START) || m_phase == Phase::WAITING_FOR_CONNECTION);

    Connection::onConnected();
}

void UnsecureConnection::start(const Endpoint& endpoint)
{
    ASSERT(!m_closed && m_phase == Phase::WAITING_FOR_START);
    ASSERT(m_outgoing && m_remoteMinerId.isValid());

    m_phase = Phase::WAITING_FOR_CONNECTION;
    m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(m_remoteMinerId.toString()));

    LOG_CLASS(debug) << "connecting to " << endpoint;

    m_readTimer.expires_after(chrono::milliseconds(getTimeout()));
    m_readTimer.async_wait([this, self = weak_from_this()](auto ec) {
        if (self.lock()) {
            onTimeout(ec);
        }
    });

    m_resolver.async_resolve(endpoint.getAddress(), std::to_string(endpoint.getPort()),
                             [this, self = shared_from_this()](auto ec, auto results) {
        if (m_closed) {
            return;
        }
        if (ec) {
            return close("can't resolve endpoint");
        }

        if (results.empty()) {
            return close("empty list of endpoints");
        }

        boost::asio::ip::tcp::endpoint endpoint = *results;
        LOG_CLASS(trace) << "async resolve found " << results.size() << " endpoints, connecting to: " << endpoint;

        m_socket.async_connect(endpoint, [this, self](auto ec) {
            if (m_closed) {
                return;
            }
            if (ec) {
                return close("can't connect");
            }

            m_socket.lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
            m_readTimer.cancel();
            start();
        });
    });
}

void UnsecureConnection::close(std::string_view reason)
{
    if (m_closed) {
        return;
    }

    m_resolver.cancel();
    boost::system::error_code ec;
    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    boost::asio::post(m_context, [this, self = shared_from_this()]() {
        boost::system::error_code ec;
        m_socket.cancel(ec);
    });

    Connection::close(reason);
}

void UnsecureConnection::read()
{
    if (m_closed) {
        return;
    }

    asio::async_read(m_socket, asio::buffer(&m_readHeader, 4), [this, self = shared_from_this()](auto ec, auto size) {
        if (m_closed) {
            return;
        }
        if (ec) {
            return close("size read error");
        }

        if (m_readHeader > getMaxPacketSize()) {
            return close("too big packet size");
        }

        auto msg = std::make_shared<Serializer>(m_readHeader);
        if (m_readHeader == 0) {
            msg->switchToReader(m_readHeader);
            return onRead(msg);
        }

        asio::async_read(m_socket, asio::buffer(msg->buffer(), m_readHeader), [this, msg, self](auto ec, auto size) {
            if (m_closed) {
                return;
            }
            if (ec) {
                return close("packet read error");
            }

            msg->switchToReader(m_readHeader);
            return onRead(msg);
        });
    });
}

void UnsecureConnection::write(const Serializer_cptr& msg)
{
    if (m_closed) {
        return;
    }

    m_writeHeader = (uint32_t)msg->pos();
    asio::async_write(m_socket, asio::buffer(&m_writeHeader, 4),
                      [this, msg, self = shared_from_this()](auto ec, auto size) {
        if (m_closed) {
            return;
        }
        if (ec) {
            return close("write size error");
        }

        asio::async_write(m_socket, asio::buffer(msg->buffer(), msg->pos()), [this, msg, self](auto ec, auto size) {
            if (m_closed) {
                return;
            }
            if (ec) {
                return close("write packet error");
            }

            onWrite();
        });
    });
}

}
