#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/storage/create_prefix.h>
#include "../transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_AUTO_TEST_SUITE(storage);
BOOST_FIXTURE_TEST_SUITE(create_prefix, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    createUser();

    auto transaction = StorageCreatePrefixTransaction::create(1, -1, "PREFIX")->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction, 1));
    auto prefix = db->unconfirmed().storage.getPrefix("PREFIX");
    BOOST_TEST_REQUIRE(prefix->getId() == "PREFIX");
    BOOST_TEST_REQUIRE(prefix->created == 1);
    BOOST_TEST_REQUIRE(prefix->owner == userIds[0]);
    BOOST_TEST_REQUIRE(prefix->entries == 0);
    BOOST_TEST_REQUIRE(prefix->lastEntry == 0);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - transaction->getFee());
}

BOOST_AUTO_TEST_CASE(duplicated_prefix)
{
    createUser();

    auto transaction1 = StorageCreatePrefixTransaction::create(1, -1, "PREFIX")->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_TEST_REQUIRE(validateAndExecute(transaction1, 1));
    auto transaction2 = StorageCreatePrefixTransaction::create(2, -1, "PREFIX")->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 2), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(invalid_prefix)
{
    createUser();

    std::string smallPrefix('X', kStoragePrefixMinLength - 1);
    auto transaction1 = StorageCreatePrefixTransaction::create(1, -1, smallPrefix)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 1), TransactionValidationError);
    std::string largePrefix('X', kStoragePrefixMaxLength + 1);
    auto transaction2 = StorageCreatePrefixTransaction::create(1, -1, largePrefix)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 1), TransactionValidationError);
    std::string invalidPrefix = "!@#$%^&*()";
    auto transaction3 = StorageCreatePrefixTransaction::create(1, -1, invalidPrefix)->
        setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction3, 1), TransactionValidationError);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
