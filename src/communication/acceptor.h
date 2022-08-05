#pragma once

#include "connection/connection.h"

namespace logpass {

class Acceptor : public std::enable_shared_from_this<Acceptor> {
public:
    Acceptor(asio::io_context& context, const boost::asio::ip::tcp::endpoint& endpoint,
             const MinerId& minerId, const std::shared_ptr<Certificate>& certificate,
             const std::function<bool(Connection_ptr)>& onConnection,
             const ConnectionCallbacks& connectionCallbacks);
    ~Acceptor();

    void open();
    void close();

    bool isClosed() const
    {
        return m_closed;
    }

    bool isOpen() const
    {
        return !m_closed;
    }

private:
    void accept();

private:
    asio::io_context& m_context;
    boost::asio::ip::tcp::endpoint m_endpoint;
    asio::ip::tcp::acceptor m_acceptor;
    asio::steady_timer m_timer;
    const MinerId m_minerId;
    const std::shared_ptr<Certificate> m_certificate;
    const std::function<bool(Connection_ptr)> m_onConnection;
    const ConnectionCallbacks m_connectionCallbacks;

    Logger m_logger;
    bool m_closed = true;
};

}
