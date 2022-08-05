#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <communication/connection/secure_connection.h>
#include <communication/connection_manager.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(connection);

BOOST_AUTO_TEST_CASE(basic)
{
    class SmallTimeoutConnection : public SecureConnection {
    public:
        using SecureConnection::SecureConnection;
        size_t getTimeout() const override
        {
            return 5000; // in milliseconds
        }
    };

    auto keys = PrivateKey::generate(5);
    Hash firstBlocksHash = Hash::generateRandom();
    asio::io_context context;
    asio::ip::tcp::acceptor acceptor(context);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 30040);
    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(100);
    std::promise<Connection_ptr> promise;
    acceptor.async_accept([&](boost::system::error_code ec, auto socket) {
        BOOST_TEST_REQUIRE(!ec);
        auto certificate = std::make_shared<Certificate>(keys[1]);
        auto connection = std::make_shared<SmallTimeoutConnection>(context, std::move(socket), certificate,
                                                                   keys[1].publicKey(), MinerId(),
                                                                   ConnectionCallbacks{
                                                                   [&](auto connection) { return; },
                                                                   [&](auto connection, auto minerId) { return true; },
                                                                   [&](auto connection) { return true; },
                                                                   [&](auto connection, auto packet) { return true; }
                                                                   });
        connection->start();
        promise.set_value(connection);
        asio::post([&] {
            acceptor.close();
        });
    });
    context.run_for(chrono::seconds(1));
    auto certificate = std::make_shared<Certificate>(keys[0]);
    auto c1 = std::make_shared<SmallTimeoutConnection>(context, asio::ip::tcp::socket(context), certificate,
                                                       keys[0].publicKey(), keys[1].publicKey(),
                                                       ConnectionCallbacks{
                                                       [&](auto connection) { return; },
                                                       [&](auto connection, auto minerId) { return true; },
                                                       [&](auto connection) { return true; },
                                                       [&](auto connection, auto packet) { return true; }
                                                       });
    c1->start(Endpoint("127.0.0.1", 30040));
    context.run_for(chrono::seconds(1));
    auto future = promise.get_future();
    Connection_ptr c2 = nullptr;
    for (int i = 0; i < 3; ++i) {
        context.run_for(chrono::seconds(1));
        if (future.wait_for(chrono::seconds(1)) == std::future_status::ready) {
            c2 = future.get();
            break;
        }
    }
    BOOST_TEST_REQUIRE(c2);
    context.run_for(chrono::milliseconds(2000 + c2->getTimeout()));
    BOOST_TEST_REQUIRE(!c2->isClosed());
    BOOST_TEST_REQUIRE(!c1->isClosed());
    c1->close();
    c2->close();
    context.run_for(chrono::seconds(1));
}

BOOST_AUTO_TEST_SUITE_END();
