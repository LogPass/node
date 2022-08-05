#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/events.h>
#include <blockchain/transactions/init.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(events);

BOOST_AUTO_TEST_CASE(basic)
{
    auto events = SharedThread<Events>();

    std::promise<Block_cptr> blockPromise;
    std::promise<Transaction_cptr> transactionPromise;

    auto listener = std::make_shared<EventsListener>(events, EventsListenerCallbacks{
        .onBlocks = [&](auto blocks, bool didChangeBranch) {
            blockPromise.set_value(blocks[0]);
        },
        .onNewTransactions = [&](auto transactions) {
            transactionPromise.set_value(transactions.front());
        }
                                                     });

    auto block = std::make_shared<Block>();
    auto transaction = std::make_shared<InitTransaction>();

    auto blockFuture = blockPromise.get_future();
    auto transactionFuture = transactionPromise.get_future();

    BOOST_REQUIRE(blockFuture.wait_for(chrono::seconds(0)) == std::future_status::timeout);
    BOOST_REQUIRE(transactionFuture.wait_for(chrono::seconds(0)) == std::future_status::timeout);

    events->onNewTransactions({ transaction });
    BOOST_REQUIRE(transactionFuture.wait_for(chrono::seconds(1)) == std::future_status::ready);
    auto transaction2 = transactionFuture.get();
    BOOST_TEST_REQUIRE(transaction == transaction2);

    events->onBlocks({ block }, false);
    BOOST_REQUIRE(blockFuture.wait_for(chrono::seconds(1)) == std::future_status::ready);
    auto block2 = blockFuture.get();
    BOOST_TEST_REQUIRE(block == block2);

    listener = nullptr;

    // if listener is not registered correctly it's going to crash because promise is already set
    events->onNewTransactions({ transaction });
    events->onBlocks({ block }, false);
}

BOOST_AUTO_TEST_CASE(threading)
{
    auto events = SharedThread<Events>();

    auto listener = std::make_shared<EventsListener>(events, EventsListenerCallbacks{
        .onBlocks = [&](auto blocks, bool didChangeBranch) {
            std::this_thread::sleep_for(chrono::seconds(2));
        },
        .onNewTransactions = [&](auto transactions) {
            std::this_thread::sleep_for(chrono::seconds(2));
        }
                                                     });

    auto block = std::make_shared<Block>();
    auto transaction = std::make_shared<InitTransaction>();

    auto start = chrono::high_resolution_clock::now();
    events->onNewTransactions({ transaction });
    events->onBlocks({ block }, false);
    BOOST_REQUIRE((chrono::high_resolution_clock::now() - start) < chrono::seconds(1));
    // if listener is not registered correctly it's going to crash because promise is already set
    events->onNewTransactions({ transaction });
    events->onBlocks({ block }, false);
}

BOOST_AUTO_TEST_SUITE_END();
