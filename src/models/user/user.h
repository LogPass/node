#pragma once

#include "user_settings.h"
#include "user_update.h"
#include "power_level.h"
#include "invalid_power_exception.h"

namespace logpass {

struct User;
using User_ptr = std::shared_ptr<User>;
using User_cptr = std::shared_ptr<const User>;

struct User {
    uint8_t version = 1;

    // base
    UserId id;
    UserId creator;
    uint64_t iteration = 0; // counts how many times model with this id has been updated
    uint32_t committedIn = 0; // block id of latest changes
    uint32_t lastSettingsUpdate = 0; // block id of latest settings changes

    // balances
    uint64_t tokens = 0;
    uint8_t freeTransactions = 0;

    // mining settings
    MinerId miner;

    // settings
    UserSettings settings;
    TransactionId settingsTransaction;

    // security
    std::set<PublicKey> lockedKeys;
    std::set<UserId> lockedSupervisors;
    uint32_t logout = 0; // block id of latest logout

    // spendings
    std::array<uint64_t, kUserPowerLevels> spendings = {};

    // update settings
    UserUpdate_cptr pendingUpdate;

    // statistics
    uint64_t operations = 0;
    uint64_t sponsors = 0;

protected:
    User() = default;
    User(const User&) = default;
    User& operator = (const User&) = delete;

public:
    // creates new user
    static User_ptr create(const PublicKey& publicKey, const UserId& creator, uint32_t blockId,
                           uint64_t balance, uint8_t freeTransactions = 0);

    // loads user
    static User_cptr load(Serializer& s, uint32_t blockId = 0);

    // creates next iteration of user
    User_ptr clone(uint32_t blockId) const;

    // serializes user
    void serialize(Serializer& s);

    // return id of user
    const UserId& getId() const
    {
        return id;
    }

    bool hasKey(const PublicKey& key) const;
    bool hasSupervisor(const UserId& supervisorId) const;

    PowerLevel getPowerLevel(const MultiSignatures& signatures, const std::set<User_cptr>& supervisors,
                             std::set<PublicKey>& usedKeys, bool ignoreLocks = false) const;

    const UserSettings& getSettings() const
    {
        return settings;
    }

    // spendings
    bool canSpendTokens(uint64_t amount, PowerLevel powerLevel) const;
    void spendTokens(uint64_t amount, PowerLevel powerLevel);

    // settings
    // throws UserSettingsValidationError
    void validateNewSettings(PowerLevel powerLevel, const UserSettings& settings) const;

    void setPendingUpdate(PowerLevel powerLevel, const UserSettings& newSettings, uint32_t blockId,
                          const TransactionId& transactionId);

    // executes pending update and clear spendings
    void executePendingUpdate(uint32_t blockId);

    // tools

    void toJSON(json& j) const;
};

}
