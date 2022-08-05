#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/transaction.h>
#include "transaction_fixture.h"

using namespace logpass;

class SimpleTransaction : public Transaction {
public:
    SimpleTransaction() : Transaction(0) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing)
    {
        auto transaction = std::make_shared<SimpleTransaction>();
        transaction->m_blockId = blockId;
        transaction->m_pricing = pricing;
        transaction->reload();
        return transaction;
    }

    constexpr virtual TransactionSettings getTransactionSettings() const
    {
        return TransactionSettings{
            .isUserManagementTransaction = true
        };
    }

protected:
    void serializeBody(Serializer& s) override {};
};

BOOST_AUTO_TEST_SUITE(transactions);
BOOST_FIXTURE_TEST_SUITE(transaction, TransactionFixture);

BOOST_AUTO_TEST_CASE(basic_execution)
{
    auto user = createUser()->clone(1);
    auto transaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUserHistory(userIds[0], 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUpdatedUserIdsCount(1) == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUpdatedUserIds(2, 0).size() == 1);
    BOOST_TEST_REQUIRE(db->unconfirmed().users.getUpdatedUserIds(2, 0).contains(userIds[0]));
}

BOOST_AUTO_TEST_CASE(free_execution)
{
    auto user = createUser()->clone(1);
    user->freeTransactions = 1;
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, 0)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->freeTransactions == 0);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
}

BOOST_AUTO_TEST_CASE(invalid_free_execution)
{
    auto user = createUser()->clone(1);
    user->freeTransactions = 0;
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, 0)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 2), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(staking)
{
    auto user = createUser()->clone(1);
    createMiner(user->getId());
    user->miner = miners[0]->getId();
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, 1)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee * 20);
    BOOST_TEST_REQUIRE(miners[0]->stake == kTransactionFee * 20);
    BOOST_TEST_REQUIRE(miners[0]->lockedStake == kTransactionFee * 20);
    BOOST_TEST_REQUIRE(miners[0]->lockedStakeBuckets.front() == kTransactionFee * 20);
}

BOOST_AUTO_TEST_CASE(staking_with_missing_miner)
{
    auto user = createUser()->clone(1);
    auto transaction = SimpleTransaction::create(1, 1)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 2), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(invalid_pricing)
{
    auto user = createUser()->clone(1);
    auto transaction1 = SimpleTransaction::create(1, -2)->setUserId(userIds[0])->sign({ keys[0] });
    auto transaction2 = SimpleTransaction::create(1, 2)->setUserId(userIds[0])->sign({ keys[0] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 2), TransactionValidationError);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 2), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(two_signatures)
{
    auto user = createUser()->clone(1);
    keys.push_back(PrivateKey::generate());
    user->settings.keys = UserKeys::create({ { keys[0].publicKey(), 1 }, { keys[1].publicKey(), 1 } });
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign(keys);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee);
}

BOOST_AUTO_TEST_CASE(invalid_public_key)
{
    auto user = createUser()->clone(1);
    keys.push_back(PrivateKey::generate());
    keys.push_back(PrivateKey::generate());
    user->settings.keys = UserKeys::create({ { keys[0].publicKey(), 1 }, { keys[1].publicKey(), 1 } });
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign(keys);
    auto transaction2 = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign({ keys[2], keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 2), TransactionValidationError);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 2), TransactionValidationError);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
}

BOOST_AUTO_TEST_CASE(sponsored)
{
    createUser();
    createUser();
    auto transaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userIds[1])->sign(keys);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - kTransactionFee);
}

BOOST_AUTO_TEST_CASE(invalid_sponsor)
{
    createUser();
    UserId userId2 = UserId(PublicKey::generateRandom());
    auto transaction1 = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userId2)->sign(keys);
    auto transaction2 = SimpleTransaction::create(1, -1)->setUserId(userId2)->setSponsorId(userIds[0])->sign(keys);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 2), TransactionValidationError);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 2), TransactionValidationError);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
}

BOOST_AUTO_TEST_CASE(sponsored_with_wrong_public_keys)
{
    createUser();
    createUser();
    keys.push_back(PrivateKey::generate());
    auto transaction1 = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userIds[1])->sign(keys);
    auto transaction2 = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userIds[1])->
        sign({ keys[0] });
    auto transaction3 = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userIds[1])->
        sign({ keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 2), TransactionValidationError);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction2, 2), TransactionValidationError);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction3, 2), TransactionValidationError);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
}

BOOST_AUTO_TEST_CASE(sponsored_with_staking)
{
    createUser();
    createUser();
    auto transaction1 = SimpleTransaction::create(1, 1)->setUserId(userIds[0])->setSponsorId(userIds[1])->sign(keys);
    BOOST_REQUIRE_THROW(validateAndExecute(transaction1, 2), TransactionValidationError);
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
}

BOOST_AUTO_TEST_CASE(key_and_supervisor)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 10);
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign(keys);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee);
}

BOOST_AUTO_TEST_CASE(only_supervisor)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 10);
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign({ keys[1] });
    BOOST_REQUIRE_THROW(validateAndExecute(transaction, 2), TransactionValidationError);
}

BOOST_AUTO_TEST_CASE(only_supervisor_as_sponsor)
{
    auto user = createUser()->clone(1);
    createUser();
    user->settings.supervisors = UserSupervisors::create(userIds[1], 10);
    updateUser(user);
    auto transaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userIds[1])->
        sign({ keys[1] });
    BOOST_REQUIRE_NO_THROW(validateAndExecute(transaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - kTransactionFee);
}

BOOST_AUTO_TEST_CASE(invalid_supervisor_power_level)
{
    auto user = createUser()->clone(1);
    auto user2 = createUser()->clone(1);
    user->settings.supervisors = UserSupervisors::create(userIds[1], 10);
    updateUser(user);
    keys.push_back(PrivateKey::generate());
    user2->settings.keys = UserKeys::create({ { keys[1].publicKey(), 1 }, { keys[2].publicKey(), 1 } });
    user2->settings.rules.powerLevels = { 1, 2, 3, 4, 5 };
    user2->settings.rules.supervisingPowerLevel = 1;
    updateUser(user2);

    auto invalidTransaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign({ keys[0], keys[1] });
    auto validTransaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign(keys);
    BOOST_REQUIRE_THROW(validateAndExecute(invalidTransaction, 2), TransactionValidationError);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(validTransaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance - kTransactionFee);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance);
}

BOOST_AUTO_TEST_CASE(minimum_power_level)
{
    auto user = createUser()->clone(1);
    keys.push_back(PrivateKey::generate());
    keys.push_back(PrivateKey::generate());
    user->settings.keys = UserKeys::create({
        { keys[0].publicKey(), 1 }, { keys[1].publicKey(), 1 }, { keys[2].publicKey(), 1 }
                                           });
    user->settings.rules.powerLevels = { 2, 3, 4, 5, 6 };
    updateUser(user);

    auto invalidTransaction1 = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign({ keys[0] });
    auto invalidTransaction2 = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign({ keys[0], keys[1] });
    auto validTransaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign(keys);
    BOOST_REQUIRE_THROW(validateAndExecute(invalidTransaction1, 2), TransactionValidationError);
    BOOST_REQUIRE_THROW(validateAndExecute(invalidTransaction2, 2), TransactionValidationError);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(validTransaction, 2));
}

BOOST_AUTO_TEST_CASE(spending_limit)
{
    auto user = createUser()->clone(1);
    keys.push_back(PrivateKey::generate());
    keys.push_back(PrivateKey::generate());
    user->settings.keys = UserKeys::create({
        { keys[0].publicKey(), 1 }, { keys[1].publicKey(), 1 }, { keys[2].publicKey(), 1 }
                                           });
    user->settings.rules.powerLevels = { 1, 1, 2, 4, 5 };
    user->settings.rules.spendingLimits = { 0, 0, user->tokens, user->tokens, user->tokens };
    updateUser(user);

    auto invalidTransaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign({ keys[0] });
    auto validTransaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->sign(keys);
    BOOST_REQUIRE_THROW(validateAndExecute(invalidTransaction, 2), TransactionValidationError);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(validTransaction, 2));
    BOOST_TEST_REQUIRE(users[0]->spendings[2] == kTransactionFee);
}

BOOST_AUTO_TEST_CASE(sponsor_spending_limit)
{
    auto user = createUser()->clone(1);
    updateUser(user);
    auto user2 = createUser()->clone(1);
    keys.push_back(PrivateKey::generate());
    keys.push_back(PrivateKey::generate());
    user2->settings.keys = UserKeys::create({
        { keys[1].publicKey(), 1 }, { keys[2].publicKey(), 1 }, { keys[3].publicKey(), 1 }
                                            });
    user2->settings.rules.powerLevels = { 1, 2, 3, 4, 5 };
    user2->settings.rules.spendingLimits = { 0, user2->tokens, user2->tokens, user2->tokens, user2->tokens };
    updateUser(user2);

    auto invalidTransaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userIds[1])->
        sign({ keys[0], keys[1] });
    auto validTransaction = SimpleTransaction::create(1, -1)->setUserId(userIds[0])->setSponsorId(userIds[1])->
        sign({ keys[0], keys[1], keys[2] });
    BOOST_REQUIRE_THROW(validateAndExecute(invalidTransaction, 2), TransactionValidationError);
    BOOST_REQUIRE_NO_THROW(validateAndExecute(validTransaction, 2));
    BOOST_TEST_REQUIRE(users[0]->tokens == kTestUserBalance);
    BOOST_TEST_REQUIRE(users[1]->tokens == kTestUserBalance - kTransactionFee);
    BOOST_TEST_REQUIRE(users[1]->spendings[1] == kTransactionFee);
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
