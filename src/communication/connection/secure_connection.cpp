#include "pch.h"

#include "secure_connection.h"

namespace logpass {

SecureConnection::SecureConnection(asio::io_context& context, asio::ip::tcp::socket&& socket,
                                   const std::shared_ptr<Certificate>& certificate,
                                   const MinerId& localMinerId, const MinerId& remoteMinerId,
                                   const ConnectionCallbacks& callbacks) :
    Connection(context, localMinerId, remoteMinerId, callbacks), m_certificate(certificate),
    m_socket(std::move(socket), certificate->getContext()), m_resolver(context)
{}

void SecureConnection::start()
{
    ASSERT(!m_closed);
    ASSERT((!m_outgoing && m_phase == Phase::WAITING_FOR_START) || m_phase == Phase::WAITING_FOR_CONNECTION);

    m_phase = Phase::WAITING_FOR_CONNECTION;
    m_socket.set_verify_callback(std::bind(&SecureConnection::verifyCertificate, this, std::placeholders::_1,
                                           std::placeholders::_2));

    m_readTimer.expires_after(chrono::milliseconds(getTimeout()));
    m_readTimer.async_wait([this, self = weak_from_this()](auto ec) {
        if (self.lock()) {
            onTimeout(ec);
        }
    });

    LOG_CLASS(trace) << "starting handshake";
    m_socket.async_handshake(m_outgoing ? boost::asio::ssl::stream_base::client : boost::asio::ssl::stream_base::server,
                             [this, self = shared_from_this()](const boost::system::error_code& ec) {
        if (ec) {
            return close("handshake failed - "s + ec.message());
        }

        m_readTimer.cancel();
        Connection::onConnected();
    });
}

void SecureConnection::start(const Endpoint& endpoint)
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
        m_socket.lowest_layer().async_connect(endpoint, [this, self](auto ec) {
            if (m_closed) {
                return;
            }
            if (ec) {
                return close("can't connect");
            }

            m_socket.lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
            m_readTimer.cancel();
            SecureConnection::start();
        });
    });
}

void SecureConnection::close(std::string_view reason)
{
    if (m_closed) {
        return;
    }

    m_resolver.cancel();
    boost::system::error_code ec;
    m_socket.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    boost::asio::post(m_context, [this, self = shared_from_this()]() {
        boost::system::error_code ec;
        m_socket.lowest_layer().cancel(ec);
    });

    Connection::close(reason);
}

bool SecureConnection::verifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx)
{
    X509* cert = X509_STORE_CTX_get0_cert(ctx.native_handle());
    if (!cert) {
        LOG_CLASS(warning) << "missing x509 certificate";
        return false;
    }

    EVP_PKEY* key = X509_get0_pubkey(cert);
    if (!key) {
        LOG_CLASS(warning) << "missing x509 public key";
        return false;
    }

    if (EVP_PKEY_base_id(key) != EVP_PKEY_ED25519) {
        LOG_CLASS(warning) << "invalid x509 public key type (" << EVP_PKEY_base_id(key) << ")";
        return false;
    }

    if (X509_verify(cert, key) != 1) {
        return false;
    }

    MinerId minerId;
    size_t len = minerId.size();
    EVP_PKEY_get_raw_public_key(key, minerId.data(), &len);

    if (m_outgoing) {
        if (minerId != m_remoteMinerId) {
            LOG_CLASS(warning) << "invalid x509 public key (minerId), got " << minerId << ", expected "
                << m_remoteMinerId;
            return false;
        }
    } else {
        if (m_remoteMinerId.isValid() && m_remoteMinerId != minerId) {
            return false;
        }
        m_remoteMinerId = minerId;
        m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(m_remoteMinerId.toString()));
        if (m_callbacks.onParams && !m_callbacks.onParams(this, m_remoteMinerId)) {
            LOG_CLASS(debug) << "onConnectionParams failed";
            return false;
        }
    }

    return true;
}

void SecureConnection::read()
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

void SecureConnection::write(const Serializer_cptr& msg)
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
