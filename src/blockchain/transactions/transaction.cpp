#include "pch.h"
#include "transaction.h"

#include <database/unconfirmed_database.h>

#include "init.h"
#include "lock_user.h"
#include "unlock_user.h"
#include "create_miner.h"
#include "create_user.h"
#include "transfer.h"
#include "increase_stake.h"
#include "logout_user.h"
#include "sponsor_user.h"
#include "update_miner.h"
#include "select_miner.h"
#include "update_user.h"
#include "commit.h"
#include "withdraw_stake.h"
#include "storage/add_entry.h"
#include "storage/create_prefix.h"
#include "storage/update_prefix.h"

namespace logpass {

Transaction_cptr Transaction::load(Serializer& s)
{
    static const std::map<uint8_t, std::function<Transaction_ptr()>> transactionSerializers = {
        // init
        {InitTransaction::TYPE, [] { return std::make_shared<InitTransaction>(); } },

        // user
        {CreateUserTransaction::TYPE, [] { return std::make_shared<CreateUserTransaction>(); }},
        {SponsorUserTransaction::TYPE, [] { return std::make_shared<SponsorUserTransaction>(); }},

        // user account management
        {UpdateUserTransaction::TYPE, [] { return std::make_shared<UpdateUserTransaction>(); }},
        {LockUserTransaction::TYPE, [] { return std::make_shared<LockUserTransaction>(); }},
        {UnlockUserTransaction::TYPE, [] { return std::make_shared<UnlockUserTransaction>(); }},
        {LogoutUserTransaction::TYPE, [] { return std::make_shared<LogoutUserTransaction>(); }},

        // transfer
        {TransferTransaction::TYPE, [] { return std::make_shared<TransferTransaction>(); }},

        // miner releated
        {CreateMinerTransaction::TYPE, [] { return std::make_shared<CreateMinerTransaction>(); }},
        {UpdateMinerTransaction::TYPE, [] { return std::make_shared<UpdateMinerTransaction>(); }},
        {SelectMinerTransaction::TYPE, [] { return std::make_shared<SelectMinerTransaction>(); }},
        {IncreaseStakeTransaction::TYPE, [] { return std::make_shared<IncreaseStakeTransaction>(); }},
        {WithdrawStakeTransaction::TYPE, [] { return std::make_shared<WithdrawStakeTransaction>(); }},

        // storage
        {StorageCreatePrefixTransaction::TYPE, [] { return std::make_shared<StorageCreatePrefixTransaction>(); }},
        {StorageUpdatePrefixTransaction::TYPE, [] { return std::make_shared<StorageUpdatePrefixTransaction>(); }},
        {StorageAddEntryTransaction::TYPE, [] { return std::make_shared<StorageAddEntryTransaction>(); }},

        // commit
        {CommitTransaction::TYPE, [] { return std::make_shared<CommitTransaction>(); }},
    };

    uint8_t transactionType = s.peek<uint8_t>();
    auto transactionIterator = transactionSerializers.find(transactionType);
    if (transactionIterator == transactionSerializers.end()) {
        THROW_SERIALIZER_EXCEPTION("Transaction has invalid type");
    }

    auto transaction = transactionIterator->second();
    transaction->serialize(s);

    if (transaction->getSize() == 0 && transaction->getSize() > kTransactionMaxSize) {
        THROW_SERIALIZER_EXCEPTION("Transaction has invalid size");
    }

    ASSERT(transaction->getType() == transactionType);
    return transaction;
}

Transaction::Transaction(uint8_t type) : m_type(type)
{}

bool Transaction::validateSignatures() const
{
    ASSERT(m_hash.isValid() && m_id.isValid());
    return m_signatures.verify(SIGNATURE_PREFIX, m_hash);
}

void Transaction::preload(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    database.users.preloadUser(getUserId());
    if (m_signatures.getType() == MultiSignaturesTypes::SPONSOR) {
        database.users.preloadUser(m_signatures.getSponsorId());
    }
}

void Transaction::validate(uint32_t blockId, const UnconfirmedDatabase& database) const
{
    // asserations
    ASSERT(m_hash.isValid() && m_id.isValid());

    // expiration
    if (blockId >= m_blockId + kTransactionMaxBlockIdDifference || m_blockId > blockId) {
        THROW_TRANSACTION_EXCEPTION("Transaction is outdated");
    }

    // duplication
    if (database.transactions.hasTransactionHash(m_blockId, getDuplicationHash())) {
        THROW_TRANSACTION_EXCEPTION("Transaction already exists");
    }

    // user validation
    User_cptr user = database.users.getUser(getUserId());
    if (!user) {
        THROW_TRANSACTION_EXCEPTION("Transaction user does not exists");
    }

    // supervisors
    std::set<User_cptr> supervisors;
    for (auto& [supervisorId, supervisorSettings] : user->settings.supervisors) {
        supervisors.insert(database.users.getUser(supervisorId));
    }

    // user power validation
    std::set<PublicKey> usedKeys;
    PowerLevel powerLevel = user->getPowerLevel(m_signatures, supervisors, usedKeys,
                                                getTransactionSettings().ignoresLock);
    if (powerLevel == PowerLevel::INVALID) {
        THROW_TRANSACTION_EXCEPTION("Transaction has invalid power level");
    }

    if (powerLevel < getTransactionSettings().minimumPowerLevel) {
        THROW_TRANSACTION_EXCEPTION("Transaction has too low power level");
    }

    // payer (sponsor) power validation
    User_cptr payer = user;
    PowerLevel payerPowerLevel;
    if (m_signatures.getType() == MultiSignaturesTypes::SPONSOR) {
        if (m_pricing > 0) {
            THROW_TRANSACTION_EXCEPTION("Transaction with sponsor can not stake transaction fees");
        }
        if (m_signatures.getUserId() == m_signatures.getSponsorId()) {
            THROW_TRANSACTION_EXCEPTION("Transaction user can not be the same as sponsor");
        }
        payer = database.users.getUser(m_signatures.getSponsorId());
        if (!payer) {
            THROW_TRANSACTION_EXCEPTION("Transaction sponsor does not exists");
        }
        payerPowerLevel = payer->getPowerLevel(m_signatures, {}, usedKeys, false);
    } else {
        // make sure that at least one of user keys has been used
        payerPowerLevel = user->getPowerLevel(m_signatures, {}, usedKeys, true);
        if (payerPowerLevel == PowerLevel::INVALID) {
            THROW_TRANSACTION_EXCEPTION("Transaction does not have any user key signature to pay for itself");
        }
        payerPowerLevel = powerLevel;
    }

    if (payerPowerLevel == PowerLevel::INVALID) {
        THROW_TRANSACTION_EXCEPTION("Transaction payer has invalid power level");
    }

    // make sure all public keys from signatures have been used
    if (usedKeys.size() != m_signatures.getSize()) {
        THROW_TRANSACTION_EXCEPTION("Not all public keys from the signature have been used");
    }

    // pricing
    if (!user->canSpendTokens(getCost(), powerLevel)) {
        THROW_TRANSACTION_EXCEPTION("Transaction user has too low balance or reached spending limits");
    }

    if (getTransactionSettings().isBlockchainManagementTransaction) {
        if (m_pricing != database.state.getPricing()) {
            THROW_TRANSACTION_EXCEPTION("Transaction has invalid pricing");
        }
    } else if (m_pricing == 0) {
        if (!getTransactionSettings().isUserManagementTransaction) {
            THROW_TRANSACTION_EXCEPTION("Transaction has invalid pricing (can not be 0)");
        }
        if (payer->freeTransactions == 0) {
            THROW_TRANSACTION_EXCEPTION("Transaction payer can't execute this transaction for free");
        }
    } else {
        if (std::abs(m_pricing) != database.state.getPricing()) {
            THROW_TRANSACTION_EXCEPTION("Transaction has diffrent than blockchain pricing");
        }
        if (m_pricing > 0 && !payer->miner.isValid()) {
            THROW_TRANSACTION_EXCEPTION("Transaction payer doesn't have miner set to use staking transaction");
        }
        uint64_t payerCost = getFee();
        if (m_signatures.getType() != MultiSignaturesTypes::SPONSOR) {
            payerCost += getCost();
        }
        if (!payer->canSpendTokens(payerCost, payerPowerLevel)) {
            THROW_TRANSACTION_EXCEPTION("Transaction payer has too low balance or reached spending limits");
        }
    }

#ifndef NDEBUG
    m_validated = blockId;
#endif
}

void Transaction::execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept
{
    // asserations
    ASSERT(m_validated == blockId);

    // adding transaction to database
    database.transactions.addTransaction(shared_from_this(), blockId);

    // paying for transaction
    User_ptr user = database.users.getUser(getUserId())->clone(blockId);

    // supervisors
    std::set<User_cptr> supervisors;
    for (auto& [supervisorId, supervisorSettings] : user->settings.supervisors) {
        supervisors.insert(database.users.getUser(supervisorId));
    }

    // user power validation
    std::set<PublicKey> usedKeys;
    PowerLevel powerLevel = user->getPowerLevel(m_signatures, supervisors, usedKeys,
                                                getTransactionSettings().ignoresLock);

    // payer (sponsor) power validation
    User_ptr payer = user;
    PowerLevel payerPowerLevel = powerLevel;
    if (m_signatures.getType() == MultiSignaturesTypes::SPONSOR) {
        payer = database.users.getUser(m_signatures.getSponsorId())->clone(blockId);
        payerPowerLevel = payer->getPowerLevel(m_signatures, {}, usedKeys, false);
    }

    if (!getTransactionSettings().isBlockchainManagementTransaction) {
        if (m_pricing == 0) {
            payer->freeTransactions -= 1;
        } else {
            payer->spendTokens(getFee(), payerPowerLevel);
            if (m_pricing > 0) {
                Miner_ptr miner = database.miners.getMiner(payer->miner)->clone(blockId);
                miner->addStake(getFee(), true);
                database.miners.updateMiner(miner);
            }
        }
    }

    // pay transaction cost
    user->spendTokens(getCost(), powerLevel);

    // payer (sponsor) history
    if (user != payer) {
        // add payer history
        database.users.addUserHistory(payer->getId(), payer->operations / 100,
                                      UserHistory(blockId, UserHistoryType::SPONSORED_TRANSACTION, getId()));
        payer->operations += 1;

        // update payer
        database.users.updateUser(payer);
    }

    // add user history
    database.users.addUserHistory(user->getId(), user->operations / 100,
                                  UserHistory(blockId, UserHistoryType::OUTGOING_TRANSACTION, getId()));
    user->operations += 1;

    // update user
    database.users.updateUser(user);
}

uint64_t Transaction::getFee() const noexcept
{
    if (m_pricing == 0) {
        return 0;
    }

    uint64_t cost = kTransactionFee * getTransactionSettings().transactionFeeMultiplier;
    if (m_pricing > 0) {
        cost = (cost * 20 * 25) / (24 + m_pricing); // staking
    } else {
        cost = (cost * 25) / (24 - m_pricing); // using tokens
    }
    return cost;
}

uint64_t Transaction::getCost() const noexcept
{
    return 0;
}

void Transaction::serialize(Serializer& s)
{
    size_t startPos = s.pos();

    s(m_type);
    s(m_blockId);
    s(m_pricing);

    serializeBody(s);

    if (!m_hash.isValid()) {
        m_hash = Hash(s.begin() + startPos, s.pos() - startPos);
    }

    s(m_signatures);

    if (!m_id.isValid()) { // updates transaction size and hash
        uint32_t size = (uint32_t)(s.pos() - startPos);
        if (size > kTransactionMaxSize) {
            THROW_SERIALIZER_EXCEPTION("Transaction excess max transaction size");
        }
        m_id = TransactionId(m_type, m_blockId, size, Hash(s.begin() + startPos, s.pos() - startPos));
    }
}

void Transaction::reload()
{
    if (!m_signatures.getUserId().isValid()) {
        Serializer s;
        s(m_type);
        s(m_blockId);
        s(m_pricing);
        serializeBody(s);
        m_hash = Hash(s);
    } else {
        m_id = TransactionId();
        Serializer finalTransactionSerializer;
        serialize(finalTransactionSerializer);
    }
}

void Transaction::toJSON(json& j) const
{
    j["id"] = m_id;
    j["hash"] = m_hash;
    j["size"] = m_id.getSize();
    j["fee"] = getFee();
    j["cost"] = getCost();

    j["type"] = m_type;
    j["block"] = m_blockId;
    j["pricing"] = m_pricing;
    j["user"] = m_signatures.getUserId();
    j["main_key"] = m_signatures.getPublicKey();
}

}
