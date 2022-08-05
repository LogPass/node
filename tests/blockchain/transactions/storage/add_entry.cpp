#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/storage/add_entry.h>
#include "../transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_AUTO_TEST_SUITE(storage);
BOOST_FIXTURE_TEST_SUITE(add_entry, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    createUser();
    createPrefix("PREFIX", userIds[0]);

    auto transaction = StorageAddEntryTransaction::create(1, -1, "PREFIX", "KEY", "VALUE")->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    auto prefix = db->unconfirmed().storage.getPrefix("PREFIX");
    auto entry = db->unconfirmed().storage.getEntry("PREFIX", "KEY");
    auto history = db->unconfirmed().storage.getTransasctionsForPrefix("PREFIX", 0);
    BOOST_TEST_REQUIRE(prefix);
    BOOST_TEST_REQUIRE(prefix->entries == 1);
    BOOST_TEST_REQUIRE(prefix->lastEntry == 1);
    BOOST_TEST_REQUIRE(entry);
    BOOST_TEST_REQUIRE(entry->id == 0);
    BOOST_TEST_REQUIRE(entry->transactionId == transaction->getId());
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee());
    BOOST_TEST_REQUIRE(history.size() == 1);
    BOOST_TEST_REQUIRE(history.front() == transaction->getId());
}

BOOST_AUTO_TEST_CASE(invalid_prefix)
{
    createUser();
    createPrefix("PREFIX", userIds[0]);
    auto transaction1 = StorageAddEntryTransaction::create(1, -1, "PREFIX1", "KEY", "VALUE")->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 1), TransactionValidationError);
    auto transaction2 = StorageAddEntryTransaction::create(1, -1, "", "KEY", "VALUE")->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(invalid_prefix_permission)
{
    createUser();
    createUser();
    createPrefix("PREFIX", userIds[0]);
    auto transaction = StorageAddEntryTransaction::create(1, -1, "PREFIX", "KEY", "VALUE")->
        setUserId(userIds[1])->sign({ keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(invalid_key)
{
    createUser();
    createPrefix("PREFIX", userIds[0]);
    auto transaction = StorageAddEntryTransaction::create(1, -1, "PREFIX", "", "VALUE")->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(invalid_value)
{
    createUser();
    createPrefix("PREFIX", userIds[0]);
    std::string value(kStorageEntryMaxValueLength + 1, 'X');
    auto transaction = StorageAddEntryTransaction::create(1, -1, "PREFIX", "KEY", value)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(max_length)
{
    createUser();
    createPrefix("PREFIX", userIds[0]);
    std::string key(255, 'X');
    std::string value(kStorageEntryMaxValueLength, 'Y');
    auto transaction = StorageAddEntryTransaction::create(1, -1, "PREFIX", key, value)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
}

BOOST_AUTO_TEST_CASE(prefix_of_other_user)
{
    createUser();
    createUser();
    auto prefix = createPrefix("PREFIX", userIds[0])->clone(1);
    prefix->settings.allowedUsers.insert(userIds[1]);
    db->unconfirmed().storage.updatePrefix(prefix);

    auto transaction = StorageAddEntryTransaction::create(1, -1, "PREFIX", "KEY", "VALUE")->
        setUserId(userIds[1])->sign({ keys[1] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    auto entry = db->unconfirmed().storage.getEntry("PREFIX", "KEY");
    BOOST_TEST_REQUIRE(entry);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - transaction->getFee());
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
