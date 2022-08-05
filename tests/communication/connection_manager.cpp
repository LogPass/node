#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <communication/connection/connection.h>
#include <communication/connection_manager.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(connection_manager);

class DummyConnection : public Connection {
public:
    DummyConnection(asio::io_context& context, const MinerId& localMinerId, const MinerId& remoteMinerId,
                    bool outgoing) :
        Connection(context, localMinerId, remoteMinerId, ConnectionCallbacks())
    {}

    ~DummyConnection()
    {
        m_closed = true;
    }

    void start() override {};
    void start(const Endpoint& endpoint) override {};

    void read() override {};
    void write(const Serializer_cptr& msg) override {};
};

/*
BOOST_AUTO_TEST_CASE(basic)
{
    auto keys = PrivateKey::generate(5);
    auto firstBlocksHash = Hash::generateRandom();
    asio::io_context context;

    ConnectionManager cm(MinerId(keys[0].publicKey()));
    auto c1 = std::make_shared<DummyConnection>(context, keys[0].publicKey(), keys[2].publicKey(), false);
    auto c2 = std::make_shared<DummyConnection>(context, keys[0].publicKey(), keys[1].publicKey(), true);
    cm.addConnection(c1.get(), MinerId());
    BOOST_TEST_REQUIRE(cm.getPendingIncomingConnectionsCount() == 1);
    cm.addConnection(c2.get(), keys[1].publicKey());
    BOOST_TEST_REQUIRE(cm.getPendingIncomingConnectionsCount() == 1);
    BOOST_TEST_REQUIRE(cm.getPendingOutgoingConnectionsCount() == 1);
    BOOST_TEST_REQUIRE(cm.canAcceptConnection(c1.get(), keys[2].publicKey()));
    cm.upgradeConnection(c1.get());
    BOOST_TEST_REQUIRE(cm.getPendingIncomingConnectionsCount() == 0);
    BOOST_TEST_REQUIRE(cm.getLowPriorityIncomingConnectionsCount() == 1);
    cm.removeConnection(c1.get());
    BOOST_TEST_REQUIRE(cm.getLowPriorityIncomingConnectionsCount() == 0);
    BOOST_TEST_REQUIRE(cm.getPendingOutgoingConnectionsCount() == 1);
    cm.upgradeConnection(c2.get());
    BOOST_TEST_REQUIRE(cm.getPendingOutgoingConnectionsCount() == 0);
    BOOST_TEST_REQUIRE(cm.getLowPriorityOutgoingConnectionsCount() == 1);
    cm.removeConnection(c2.get());
    BOOST_TEST_REQUIRE(cm.getLowPriorityOutgoingConnectionsCount() == 0);
}
*/

BOOST_AUTO_TEST_SUITE_END();
