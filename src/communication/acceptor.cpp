#include "pch.h"

#include "acceptor.h"
#include "connection/secure_connection.h"
#include "connection/unsecure_connection.h"

namespace logpass {

Acceptor::Acceptor(asio::io_context& context, const boost::asio::ip::tcp::endpoint& endpoint,
                   const MinerId& minerId, const std::shared_ptr<Certificate>& certificate,
                   const std::function<bool(Connection_ptr)>& onConnection,
                   const ConnectionCallbacks& connectionCallbacks) :
    m_context(context), m_endpoint(endpoint), m_minerId(minerId), m_certificate(certificate),
    m_onConnection(onConnection), m_connectionCallbacks(connectionCallbacks), m_acceptor(m_context), m_timer(context)
{
    ASSERT(m_onConnection && m_minerId.isValid());
    m_logger.add_attribute("Class", boost::log::attributes::constant<std::string>("Acceptor"));
    m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(
        endpoint.address().to_string() + ":"s + std::to_string(endpoint.port())));
}

Acceptor::~Acceptor()
{
    ASSERT(m_closed);
}

void Acceptor::open()
{
    m_closed = false;
    try {
        m_timer.cancel();
        if (m_acceptor.is_open()) {
            m_acceptor.cancel();
            m_acceptor.close();
        }
        m_acceptor.open(m_endpoint.protocol());
        m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        m_acceptor.set_option(boost::asio::ip::tcp::no_delay(true));
        m_acceptor.bind(m_endpoint);
        m_acceptor.listen(100);

        LOG_CLASS(info) << "Acceptor started on " << m_endpoint;
        accept();
    } catch (const boost::system::system_error& error) {
        LOG_CLASS(error) << "Acceptor can't bind " << m_endpoint << " (" << error.what() <<
            "). Trying again in 1 second";
        m_timer.expires_after(chrono::seconds(1));
        m_timer.async_wait([this](auto ec) { if (!ec && !m_closed) open(); });
    }
}

void Acceptor::close()
{
    if (m_closed) {
        return;
    }

    m_closed = true;
    LOG_CLASS(debug) << "closing";

    boost::system::error_code ec;
    m_acceptor.cancel(ec);
    m_acceptor.close(ec);
    m_timer.cancel(ec);
}

void Acceptor::accept()
{
    if (m_closed) {
        return;
    }

    m_acceptor.async_accept([this, self = shared_from_this()](boost::system::error_code ec, auto socket) {
        if (m_closed) {
            return;
        }

        if (ec) {
            LOG_CLASS(warning) << "async_accept error: " << ec.message();
            return open();
        }

        LOG_CLASS(debug) << "incoming connection from " << socket.remote_endpoint(ec);
        Connection_ptr connection;
        if (m_certificate) {
            connection = std::make_shared<SecureConnection>(m_context, std::move(socket), m_certificate, m_minerId,
                                                            MinerId(), m_connectionCallbacks);
        } else {
            connection = std::make_shared<UnsecureConnection>(m_context, std::move(socket), m_minerId, MinerId(),
                                                              m_connectionCallbacks);
        }

        if (!m_onConnection(connection)) {
            LOG_CLASS(debug) << "connection was declined, pausing acceptor for 1s";
            connection->close("not accepted");
            m_acceptor.close();
            m_timer.expires_after(chrono::seconds(1));
            m_timer.async_wait([this](auto ec) { if (!ec && !m_closed) open(); });
            return;
        }

        accept();
    });
}

}
