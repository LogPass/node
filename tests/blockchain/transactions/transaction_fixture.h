#pragma once

#include <database/database_fixture.h>
#include <models/user/user.h>
#include <models/miner/miner.h>
#include <models/storage/prefix.h>

namespace logpass {

struct TransactionFixture : public DatabaseFixture<> {
    static constexpr uint64_t kTestUserBalance = 1'000'000'000'000'000; // 1 mln tokens

    std::vector<PrivateKey> keys;
    std::vector<User_cptr> users;
    std::vector<UserId> userIds;
    std::vector<Miner_cptr> miners;

    struct CreateUserParams {
        PrivateKey key = PrivateKey::generate();
        UserId creator = UserId();
        uint32_t blockId = 1;
        uint64_t tokens = kTestUserBalance;
        uint8_t freeTransaction = kUserMinFreeTransactions;
    };

    // default parameter for createUser is not used due to gcc/clang bug
    // https://bugs.llvm.org/show_bug.cgi?id=36684
    User_cptr createUser()
    {
        return createUser(CreateUserParams());
    }

    User_cptr createUser(const CreateUserParams& params)
    {
        keys.push_back(params.key);
        auto user = User::create(params.key.publicKey(), params.creator, params.blockId, params.tokens,
            params.freeTransaction);
        db->unconfirmed().users.addUser(user);
        users.push_back(user);
        userIds.push_back(user->getId());
        return user;
    }

    Miner_cptr createMiner(const UserId& owner, uint32_t blockId = 1)
    {
        auto miner = Miner::create(keys[miners.size()].publicKey(), owner, blockId);
        db->unconfirmed().miners.addMiner(miner);
        miners.push_back(miner);
        return miner;
    }

    Prefix_cptr createPrefix(const std::string& id, const UserId& owner, uint32_t blockId = 1)
    {
        auto prefix = Prefix::create(id, owner, blockId);
        db->unconfirmed().storage.addPrefix(prefix);
        return prefix;
    }

    bool validateAndExecute(Transaction_cptr transaction, uint32_t blockId)
    {
        if (transaction->getType() != 0) {
            TransactionId id = transaction->getId();
            // reload transaction to catch serialization errors
            Serializer s;
            s(transaction);
            s.switchToReader();
            transaction = Transaction::load(s);
            if (transaction->getId() != id) {
                THROW_SERIALIZER_EXCEPTION("Transaction has different Id after serialization");
            }
        }

        if (!transaction->validateSignatures()) {
            THROW_TRANSACTION_EXCEPTION("Signature validation failed");
        }
        transaction->validate(blockId, db->unconfirmed());
        transaction->execute(blockId, db->unconfirmed());
        ASSERT(db->unconfirmed().transactions.hasTransactionHash(transaction->getBlockId(),
            transaction->getDuplicationHash()));
        ASSERT(db->unconfirmed().transactions.getTransaction(transaction->getId()));
        refresh();
        try {
            transaction->validate(blockId, db->unconfirmed());
        } catch (const TransactionValidationError&) {
            return true;
        }
        THROW_TRANSACTION_EXCEPTION("Transaction is still valid after execution");
    }

    void updateUser(const User_ptr& user)
    {
        db->unconfirmed().users.updateUser(user);
        refresh();
    }

    void updateMiner(const Miner_ptr& miner)
    {
        db->unconfirmed().miners.updateMiner(miner);
        refresh();
    }

    void refresh()
    {
        for (auto& user : users) {
            user = db->unconfirmed().users.getUser(user->getId());
        }
        for (auto& miner : miners) {
            miner = db->unconfirmed().miners.getMiner(miner->getId());
        }
    }

};

}
