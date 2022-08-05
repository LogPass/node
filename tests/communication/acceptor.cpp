#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <communication/acceptor.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(acceptor);

class SimpleConnection {
public:
    SimpleConnection(boost::asio::io_context& context, boost::asio::ip::tcp::endpoint& endpoint) :
        m_socket(context)
    {
        boost::asio::post(context, [this, endpoint] {
            boost::system::error_code ec;
            m_socket.connect(endpoint, ec);
            connected = !ec;
        });
    }

    bool connected = false;
    boost::asio::ip::tcp::socket m_socket;
};

BOOST_AUTO_TEST_CASE(connection)
{
    auto keys = PrivateKey::generate(2);
    boost::asio::io_context context;
    auto guard = boost::asio::make_work_guard(context);
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 30010);
    auto acceptor = std::make_shared<Acceptor>(context, endpoint, MinerId(keys[0].publicKey()), nullptr,
                                               [](auto connection) { return false; }, ConnectionCallbacks());
    acceptor->open();
    context.run_for(chrono::milliseconds(1000));
    BOOST_TEST(acceptor->isOpen());
    context.run_for(chrono::milliseconds(1000));
    SimpleConnection c1(context, endpoint);
    context.run_for(chrono::milliseconds(3000));
    BOOST_TEST(c1.connected);
    BOOST_TEST(acceptor->isOpen());
    SimpleConnection c2(context, endpoint);
    context.run_for(chrono::milliseconds(3000));
    BOOST_TEST(c2.connected);
    BOOST_TEST(acceptor->isOpen());
    acceptor->close();
    BOOST_TEST(acceptor->isClosed());
    guard.reset();
    context.run_for(chrono::milliseconds(1000));
}

BOOST_AUTO_TEST_CASE(duplicated)
{
    auto keys = PrivateKey::generate(2);
    boost::asio::io_context context;
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 30010);
    auto acceptor1 = std::make_shared<Acceptor>(context, endpoint, MinerId(keys[0].publicKey()), nullptr,
                                                [](auto connection) { return false; }, ConnectionCallbacks());
    auto acceptor2 = std::make_shared<Acceptor>(context, endpoint, MinerId(keys[1].publicKey()), nullptr,
                                                [](auto connection) { return false; }, ConnectionCallbacks());

    auto guard = boost::asio::make_work_guard(context);
    boost::asio::post(context, [&] {
        acceptor1->open();
        acceptor2->open();
    });
    context.run_for(chrono::milliseconds(2000));
    boost::asio::post(context, [&] {
        acceptor1->close();
        acceptor2->close();
    });
    context.run_for(chrono::milliseconds(2000));
    boost::asio::post(context, [&] {
        acceptor1->open();
        acceptor2->open();
    });
    context.run_for(chrono::milliseconds(2000));
    boost::asio::post(context, [&] {
        acceptor1->close();
    });
    context.run_for(chrono::milliseconds(2000));
    SimpleConnection c1(context, endpoint);
    context.run_for(chrono::milliseconds(2000));
    BOOST_TEST(c1.connected);
    BOOST_TEST(acceptor1->isClosed());
    BOOST_TEST(acceptor2->isOpen());
    boost::asio::post(context, [&] {
        acceptor2->close();
        guard.reset();
    });
    context.run_for(chrono::milliseconds(2000));
}

BOOST_AUTO_TEST_SUITE_END();
