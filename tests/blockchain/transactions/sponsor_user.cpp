#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/sponsor_user.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(sponsor_user, TransactionFixture);

BOOST_AUTO_TEST_CASE(execution)
{
    createUser();
    createUser();

    Hash sponsorHash = Hash::generateRandom();

    auto transaction = SponsorUserTransaction::create(1, -1, userIds[1], kUserMinFreeTransactions, sponsorHash)->
        setUserId(userIds[0])->sign(keys[0]);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));

    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee * (1 + kUserMinFreeTransactions));
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(users[0]->sponsors == 0);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMinFreeTransactions * 2);
    BOOST_TEST_REQUIRE(users[1]->sponsors == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[1], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[0], 0).size() == 0);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[1], 0).size() == 1);
    UserSponsor userSponsor = db->unconfirmed().users.getUserSponsors(userIds[1], 0).front();
    BOOST_TEST_REQUIRE(userSponsor.blockId == 2);
    BOOST_TEST_REQUIRE(userSponsor.sponsor == sponsorHash);
    BOOST_TEST_REQUIRE(userSponsor.transactions == kUserMinFreeTransactions);
}

BOOST_AUTO_TEST_CASE(max_free_transactions)
{
    createUser();
    createUser();

    Hash sponsorHash = Hash::generateRandom();

    auto transaction = SponsorUserTransaction::create(1, -1, userIds[1], kUserMaxFreeTransactions, sponsorHash)->
        setUserId(userIds[0])->sign(keys[0]);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));

    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee * (1 + kUserMaxFreeTransactions));
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(users[0]->sponsors == 0);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMaxFreeTransactions);
    BOOST_TEST_REQUIRE(users[1]->sponsors == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[0], 0).size() == 0);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[1], 0).size() == 1);
    UserSponsor userSponsor = db->unconfirmed().users.getUserSponsors(userIds[1], 0).front();
    BOOST_TEST_REQUIRE(userSponsor.blockId == 2);
    BOOST_TEST_REQUIRE(userSponsor.sponsor == sponsorHash);
    BOOST_TEST_REQUIRE(userSponsor.transactions == kUserMaxFreeTransactions);
}

BOOST_AUTO_TEST_CASE(renewing_itself)
{
    createUser();

    Hash sponsorHash = Hash::generateRandom();

    auto transaction = SponsorUserTransaction::create(1, -1, userIds[0], kUserMinFreeTransactions, sponsorHash)->
        setUserId(userIds[0])->sign(keys[0]);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));

    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee * (1 + kUserMinFreeTransactions));
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions * 2);
    BOOST_TEST_REQUIRE(users[0]->sponsors == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 2);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[0], 0).size() == 1);
    UserSponsor userSponsor = db->unconfirmed().users.getUserSponsors(userIds[0], 0).front();
    BOOST_TEST_REQUIRE(userSponsor.blockId == 2);
    BOOST_TEST_REQUIRE(userSponsor.sponsor == sponsorHash);
    BOOST_TEST_REQUIRE(userSponsor.transactions == kUserMinFreeTransactions);
}

BOOST_AUTO_TEST_CASE(sponsored)
{
    createUser();
    createUser();
    createUser();

    auto transaction = SponsorUserTransaction::create(1, -1, userIds[1], kUserMinFreeTransactions)->
        setUserId(userIds[0])->setSponsorId(userIds[2])->sign({ keys[0], keys[2] });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));

    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMinFreeTransactions * 2);
    BOOST_TEST_REQUIRE(users[2]->tokens == kTestUserBalance - kTransactionFee * (1 + kUserMinFreeTransactions));
    BOOST_TEST_REQUIRE(users[2]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[1], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[2], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[0], 0).size() == 0);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[1], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[2], 0).size() == 0);
}

BOOST_AUTO_TEST_CASE(sponsored_rewnewing_itself)
{
    createUser();
    createUser();

    auto transaction = SponsorUserTransaction::create(1, -1, userIds[0], kUserMinFreeTransactions)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));

    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions * 2);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - kTransactionFee * (1 + kUserMinFreeTransactions));
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 2);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[1], 0).size() == 1);
}

BOOST_AUTO_TEST_CASE(two_sponsors)
{
    createUser();
    createUser();

    Hash sponsorHash1 = Hash::generateRandom();
    Hash sponsorHash2 = Hash::generateRandom();

    auto transaction1 = SponsorUserTransaction::create(1, -1, userIds[1], kUserMinFreeTransactions, sponsorHash1)->
        setUserId(userIds[0])->sign(keys[0]);
    auto transaction2 = SponsorUserTransaction::create(1, -1, userIds[1], kUserMaxFreeTransactions / 2, sponsorHash2)->
        setUserId(userIds[0])->sign(keys[0]);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction1, 2));
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction2, 2));

    uint64_t transactionFeeMultiplier = 2 + (uint64_t)kUserMinFreeTransactions + (uint64_t)kUserMaxFreeTransactions / 2;
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee * transactionFeeMultiplier);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(users[0]->sponsors == 0);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMinFreeTransactions * 2 + kUserMaxFreeTransactions / 2);
    BOOST_TEST_REQUIRE(users[1]->sponsors == 2);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 2);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[1], 0).size() == 2);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[0], 0).size() == 0);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[1], 0).size() == 2);
    UserSponsor userSponsor1 = db->unconfirmed().users.getUserSponsors(userIds[1], 0)[0];
    BOOST_TEST_REQUIRE(userSponsor1.blockId == 2);
    BOOST_TEST_REQUIRE(userSponsor1.sponsor == sponsorHash1);
    BOOST_TEST_REQUIRE(userSponsor1.transactions == kUserMinFreeTransactions);
    UserSponsor userSponsor2 = db->unconfirmed().users.getUserSponsors(userIds[1], 0)[1];
    BOOST_TEST_REQUIRE(userSponsor2.blockId == 2);
    BOOST_TEST_REQUIRE(userSponsor2.sponsor == sponsorHash2);
    BOOST_TEST_REQUIRE(userSponsor2.transactions == kUserMaxFreeTransactions / 2);
}

BOOST_AUTO_TEST_CASE(invalid_sponsoring)
{
    createUser();
    createUser();

    auto transaction = SponsorUserTransaction::create(1, -1, userIds[1], kUserMinFreeTransactions)->
        setUserId(userIds[0])->setSponsorId(userIds[1])->sign({ keys[0], keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 2), TransactionValidationError);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMinFreeTransactions);
}

BOOST_AUTO_TEST_CASE(invalid_free_transactions)
{
    createUser();
    createUser();

    auto transaction1 = SponsorUserTransaction::create(1, -1, userIds[1], kUserMaxFreeTransactions + 1)->
        setUserId(userIds[0])->sign(keys[0]);
    auto transaction2 = SponsorUserTransaction::create(1, -1, userIds[1], 0)->
        setUserId(userIds[0])->sign(keys[0]);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 2), TransactionValidationError);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 2), TransactionValidationError);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMinFreeTransactions);
}

BOOST_AUTO_TEST_CASE(invalid_pricing)
{
    createUser();
    createUser();

    auto transaction = SponsorUserTransaction::create(1, 0, userIds[1], kUserMaxFreeTransactions + 1)->
        setUserId(userIds[0])->sign(keys[0]);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 2), TransactionValidationError);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->freeTransactions == kUserMinFreeTransactions);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
