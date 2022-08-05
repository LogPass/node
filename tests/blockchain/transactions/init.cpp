#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/init.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(init, TransactionFixture);

BOOST_AUTO_TEST_CASE(execution)
{
    auto key = PrivateKey::generate();
    uint64_t initializationTime = (time(nullptr) / 60) * 60;
    auto initTransaction = InitTransaction::create(1, 0, initializationTime, kBlockInterval);
    initTransaction->setUserId(key.publicKey())->sign({ key });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(initTransaction, 1));
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUser(UserId(key.publicKey())));
    BOOST_TEST_REQUIRE(db->unconfirmed().transactions.hasTransactionHash(initTransaction->getBlockId(),
                                                                         initTransaction->getDuplicationHash()));
    BOOST_TEST_REQUIRE(db->unconfirmed().transactions.getTransaction(initTransaction->getId()));
    BOOST_TEST_REQUIRE(db->unconfirmed().transactions.getTransactionsCountByType(initTransaction->getType()) == 1);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
