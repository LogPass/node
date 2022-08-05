#pragma once

#include "connection.h"

namespace logpass {

// not thread-safe
class SecureConnection : public Connection {
public:
    SecureConnection(asio::io_context& context, asio::ip::tcp::socket&& socket,
                     const std::shared_ptr<Certificate>& certificate,
                     const MinerId& localMinerId, const MinerId& remoteMinerId, const ConnectionCallbacks& callbacks);

    void start() override;
    // starts working, for outgoing connection, should be called only once
    void start(const Endpoint& endpoint) override;
    // closes connection
    void close(std::string_view reason = "") override;

protected:
    bool verifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx);
    void read() override;
    void write(const Serializer_cptr& msg) override;

protected:
    const std::shared_ptr<Certificate> m_certificate;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;
    boost::asio::ip::tcp::resolver m_resolver;
    uint32_t m_readHeader = 0, m_writeHeader = 0;
};

}
