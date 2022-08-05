#pragma once

#include <blockchain/blockchain.h>
#include <blockchain/transactions/lock_user.h>
#include <blockchain/transactions/increase_stake.h>
#include <blockchain/transactions/withdraw_stake.h>
#include <blockchain/transactions/create_miner.h>
#include <blockchain/transactions/create_user.h>
#include <blockchain/transactions/logout_user.h>
#include <blockchain/transactions/select_miner.h>
#include <blockchain/transactions/transfer.h>
#include <blockchain/transactions/unlock_user.h>
#include <blockchain/transactions/update_miner.h>
#include <blockchain/transactions/update_user.h>
#include <blockchain/transactions/storage/add_entry.h>
#include <blockchain/transactions/storage/create_prefix.h>
#include <blockchain/transactions/storage/update_prefix.h>
#include <database/database.h>

namespace logpass {

class TestUser {
public:
    TestUser() = default;
    TestUser(const UserId& userId, const std::vector<PrivateKey>& keys,
             const std::weak_ptr<BlockchainTest>& blockchain, const std::weak_ptr<Database>& database) :
        m_userId(userId), m_keys(keys), m_blockchain(blockchain), m_database(database)
    {
        update();
    }

    void setBlockchain(const std::shared_ptr<BlockchainTest>& blockchain)
    {
        m_blockchain = blockchain;
    }

    void setDatabase(const std::shared_ptr<Database>& database)
    {
        m_database = database;
    }

    const std::vector<PrivateKey> getKeys() const
    {
        return m_keys;
    }

    void setKeys(const std::vector<PrivateKey>& keys)
    {
        m_keys = keys;
    }

    void setSponsor(const UserId& sponsorId)
    {
        m_sponsorId = sponsorId;
    }

    void setSponsor(const TestUser& sponsor)
    {
        m_sponsorId = sponsor.getId();
    }

    void clearSponsor()
    {
        m_sponsorId = UserId();
    }

    void setBlockId(uint32_t blockId)
    {
        m_blockId = blockId;
    }

    void setPricing(int16_t pricing)
    {
        m_pricing = pricing;
    }

    const User& operator*()
    {
        update();
        ASSERT(m_user);
        return *m_user;
    }

    const User* operator->()
    {
        update();
        ASSERT(m_user);
        return m_user.get();
    }

    operator const UserId() const
    {
        return m_userId;
    }

    const User_cptr confirmed()
    {
        return m_database.lock()->confirmed().users.getUser(m_userId);
    }

    UserId getId() const
    {
        return m_userId;
    }

    TestUser createUser(const PrivateKey& key = PrivateKey::generate())
    {
        auto status = createUser(key.publicKey());
        ASSERT(status);
        return TestUser(UserId(key.publicKey()), { key }, m_blockchain, m_database);
    }

    std::vector<TestUser> createUsers(size_t count)
    {
        std::vector<TestUser> users;
        for (auto& key : PrivateKey::generate(count)) {
            users.emplace_back(createUser(key));
        }
        return users;
    }

    PostTransactionResult lockUser(const std::set<PublicKey>& keysToLock, const std::set<UserId>& supervisorsToLock)
    {
        return m_blockchain.lock()->postTransaction(
            LockUserTransaction::create(getBlockId(), 0, keysToLock, supervisorsToLock)->
            setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult unlockUser(const std::set<PublicKey>& keysToUnlock,
                                     const std::set<UserId>& supervisorsToUnlock)
    {
        return m_blockchain.lock()->postTransaction(
            UnlockUserTransaction::create(getBlockId(), 0, keysToUnlock, supervisorsToUnlock)->
            setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult increaseStake(const MinerId& minerId, uint64_t value)
    {
        return m_blockchain.lock()->postTransaction(
            IncreaseStakeTransaction::create(getBlockId(), m_pricing, minerId, value)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult withdrawStake(const MinerId& minerId, uint64_t unlockedStake, uint64_t lockedStake)
    {
        return m_blockchain.lock()->postTransaction(
            WithdrawStakeTransaction::create(getBlockId(), m_pricing, minerId, unlockedStake, lockedStake)->
            setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult createUser(const PublicKey& publicKey,
                                     uint8_t sponsoredTransactions = kUserMinFreeTransactions,
                                     const Hash& sponsor = Hash())
    {
        return m_blockchain.lock()->postTransaction(
            CreateUserTransaction::create(getBlockId(), m_pricing, publicKey, sponsoredTransactions, sponsor)->
            setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult createMiner(const MinerId& minerId)
    {
        return m_blockchain.lock()->postTransaction(
            CreateMinerTransaction::create(getBlockId(), m_pricing, minerId)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult logoutUser()
    {
        return m_blockchain.lock()->postTransaction(
            LogoutUserTransaction::create(getBlockId(), m_pricing)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult transferTokens(const UserId& destination, uint64_t value)
    {
        return m_blockchain.lock()->postTransaction(
            TransferTransaction::create(getBlockId(), m_pricing, destination, value)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult selectMiner(const MinerId& minerId)
    {
        return m_blockchain.lock()->postTransaction(
            SelectMinerTransaction::create(getBlockId(), m_pricing, minerId)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult updateMiner(const MinerId& minerId, const MinerSettings& settings)
    {
        return m_blockchain.lock()->postTransaction(
            UpdateMinerTransaction::create(getBlockId(), m_pricing, minerId, settings)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult updateUser(const UserSettings& settings)
    {
        return m_blockchain.lock()->postTransaction(
            UpdateUserTransaction::create(getBlockId(), m_pricing, settings)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult createPrefix(const std::string& prefixId)
    {
        return m_blockchain.lock()->postTransaction(
            StorageCreatePrefixTransaction::create(getBlockId(), m_pricing, prefixId)->setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult updatePrefix(const std::string& prefixId, const PrefixSettings& settings)
    {
        return m_blockchain.lock()->postTransaction(
            StorageUpdatePrefixTransaction::create(getBlockId(), m_pricing, prefixId, settings)->
            setUserId(m_userId)->sign(m_keys)
        );
    }

    PostTransactionResult addStorageEntry(const std::string& prefix, const std::string& key, const std::string& value)
    {
        return m_blockchain.lock()->postTransaction(
            StorageAddEntryTransaction::create(getBlockId(), m_pricing, prefix, key, value)->
            setUserId(m_userId)->sign(m_keys)
        );
    }

protected:

    void update()
    {
        m_user = m_database.lock()->unconfirmed().users.getUser(m_userId);
    }

    uint32_t getBlockId() const
    {
        if (m_blockId != 0) {
            return m_blockId;
        }
        return m_blockchain.lock()->getLatestBlockId();
    }

    UserId m_userId;
    UserId m_sponsorId;
    std::vector<PrivateKey> m_keys;
    std::weak_ptr<BlockchainTest> m_blockchain;
    std::weak_ptr<Database> m_database;
    User_cptr m_user;
    uint32_t m_blockId = 0;
    int16_t m_pricing = -1;
};

}
