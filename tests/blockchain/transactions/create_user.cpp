#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/create_user.h>
#include "transaction_fixture.h"

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(create_user, TransactionFixture);

BOOST_AUTO_TEST_CASE(execution)
{
    createUser();
    keys.push_back(PrivateKey::generate());

    auto sponsorHash = Hash::generateRandom();
    auto transaction = CreateUserTransaction::create(1, -1, keys[1].publicKey(), kUserMinFreeTransactions, sponsorHash);
    transaction->setUserId(userIds[0])->sign(keys[0]);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUser(keys[1].publicKey()));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - (kTransactionFee * (1 + kUserMinFreeTransactions)));
    User_cptr user = db->unconfirmed().users.getUser(keys[1].publicKey());
    BOOST_TEST_REQUIRE(user);
    BOOST_TEST_REQUIRE(user->tokens == 0);
    BOOST_TEST_REQUIRE(user->freeTransactions == kUserMinFreeTransactions);
    BOOST_TEST_REQUIRE(user->creator == userIds[0]);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(userIds[0], 0).size() == 0);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(user->getId(), 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserSponsors(user->getId(), 0).size() == 1);
    UserSponsor userSponsor = db->unconfirmed().users.getUserSponsors(user->getId(), 0).front();
    BOOST_TEST_REQUIRE(userSponsor.blockId == 2);
    BOOST_TEST_REQUIRE(userSponsor.sponsor == sponsorHash);
    BOOST_TEST_REQUIRE(userSponsor.transactions == kUserMinFreeTransactions);
}

BOOST_AUTO_TEST_CASE(serialization)
{
    auto privateKey = PrivateKey::generate();
    auto privateKey2 = PrivateKey::generate();
    auto transaction = CreateUserTransaction::create(1, -1, privateKey2.publicKey(), kUserMinFreeTransactions);
    transaction->setUserId(privateKey.publicKey())->setPublicKey(privateKey.publicKey())->sign(privateKey);
    BOOST_TEST_REQUIRE(transaction->getType() == CreateUserTransaction::TYPE);
    BOOST_TEST_REQUIRE(transaction->getFee() == kTransactionFee * (1 + kUserMinFreeTransactions));
    BOOST_TEST_REQUIRE(transaction->getBlockId() == 1);
    BOOST_TEST_REQUIRE(transaction->getUserId() == UserId(privateKey.publicKey()));
    BOOST_TEST_REQUIRE(transaction->getId().isValid());
    BOOST_TEST_REQUIRE(transaction->getDuplicationHash().isValid());
    Serializer s;
    s(*transaction);
    s.switchToReader();
    auto transaction2 = Transaction::load(s);
    BOOST_TEST_REQUIRE(transaction2);
    BOOST_TEST_REQUIRE(transaction->getType() == transaction2->getType());
    BOOST_TEST_REQUIRE(transaction->getUserId() == transaction2->getUserId());
    BOOST_TEST_REQUIRE(transaction->getBlockId() == transaction2->getBlockId());
    BOOST_TEST_REQUIRE(transaction->getId() == transaction2->getId());
    BOOST_TEST_REQUIRE(transaction->getDuplicationHash() == transaction2->getDuplicationHash());
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
