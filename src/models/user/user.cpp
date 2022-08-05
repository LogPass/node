#include "pch.h"
#include "user.h"

namespace logpass {

User_ptr User::create(const PublicKey& publicKey, const UserId& creator, uint32_t blockId, uint64_t tokens,
                      uint8_t freeTransactions)
{
    User_ptr user(new User);
    user->id = UserId(publicKey);
    user->creator = creator;
    user->committedIn = blockId;
    user->lastSettingsUpdate = blockId;
    user->settings.keys = UserKeys::create(publicKey);
    user->tokens = tokens;
    user->freeTransactions = freeTransactions;
    return user;
}

User_cptr User::load(Serializer& s, uint32_t blockId)
{
    User_ptr user(new User);
    s(*user);
    if (user->pendingUpdate && blockId >= user->pendingUpdate->blockId) {
        user->executePendingUpdate(blockId);
    }
    return user;
}

User_ptr User::clone(uint32_t blockId) const
{
    User_ptr user = std::shared_ptr<User>(new User(*this));
    user->iteration = iteration + 1;
    user->committedIn = blockId;
    return user;
}

bool User::hasKey(const PublicKey& key) const
{
    return settings.keys.hasKey(key);
}

bool User::hasSupervisor(const UserId& supervisorId) const
{
    return settings.supervisors.hasSupervisor(supervisorId);
}

PowerLevel User::getPowerLevel(const MultiSignatures& signatures, const std::set<User_cptr>& supervisors,
                               std::set<PublicKey>& usedKeys, bool ignoreLocks) const
{
    uint16_t rawPower = 0;
    uint8_t participants = 0;
    bool hasLockedKeyOrSupervisor = false;
    for (auto& [key, settings] : settings.keys) {
        if (signatures.contains(key)) {
            rawPower += settings.power;
            participants += 1;
            usedKeys.insert(key);
            if (lockedKeys.contains(key)) {
                hasLockedKeyOrSupervisor = true;
            }
        }
    }

    for (auto& supervisor : supervisors) {
        ASSERT(settings.supervisors.hasSupervisor(supervisor->getId()));
        std::set<PublicKey> supervisorUsedKeys;
        PowerLevel supervisorPowerLevel = supervisor->getPowerLevel(signatures, {}, supervisorUsedKeys);
        if (supervisorPowerLevel >= PowerLevel(supervisor->getSettings().rules.supervisingPowerLevel)) {
            rawPower += settings.supervisors.getSupervisorSettings(supervisor->getId()).power;
            participants += 1;
            usedKeys.insert(supervisorUsedKeys.begin(), supervisorUsedKeys.end());
            if (lockedSupervisors.contains(supervisor->getId())) {
                hasLockedKeyOrSupervisor = true;
            }
        }
    }

    if (rawPower == 0 || rawPower < settings.rules.powerLevels[0]) {
        return PowerLevel::INVALID;
    }

    uint8_t level = 0;
    for (int8_t i = kUserPowerLevels - 1; i >= 0; --i) {
        if (rawPower >= settings.rules.powerLevels[i]) {
            level = i;
            break;
        }
    }

    PowerLevel powerLevel(level, std::min<uint16_t>(kMaxPower, rawPower), participants);
    if (!ignoreLocks && hasLockedKeyOrSupervisor && powerLevel < PowerLevel::MEDIUM) {
        return PowerLevel::INVALID;
    }
    return powerLevel;
}

bool User::canSpendTokens(uint64_t amount, PowerLevel powerLevel) const
{
    if (powerLevel == PowerLevel::INVALID) {
        return false;
    }

    if (amount == 0) {
        return true;
    }

    if (amount > tokens) {
        return false;
    }

    if (settings.rules.spendingLimits[kUserPowerLevels - 1] != 0) {
        // no limit set
        for (uint8_t i = powerLevel.getIndex(); i < kUserPowerLevels; ++i) {
            if (spendings[i] + amount > settings.rules.spendingLimits[i]) {
                return false;
            }
        }
    }

    return true;
}

void User::spendTokens(uint64_t amount, PowerLevel powerLevel)
{
    ASSERT(tokens >= amount);
    ASSERT(powerLevel != PowerLevel::INVALID);

    // remove tokens
    tokens -= amount;

    // increase spendings
    spendings[powerLevel.getIndex()] += amount;

    // clear lower spendings
    for (uint8_t i = 0; i < powerLevel.getIndex(); ++i) {
        spendings[i] = 0;
    }
}

void User::validateNewSettings(PowerLevel powerLevel, const UserSettings& newSettings) const
{
    newSettings.validate();

    if (newSettings.supervisors.hasSupervisor(id)) {
        THROW_USER_SETTINGS_EXCEPTION("User can't have himself as supervisor");
    }

    if (pendingUpdate && pendingUpdate->powerLevel > powerLevel) {
        THROW_USER_SETTINGS_EXCEPTION("Not enough power to superpass current pending update");
    }
}

void User::setPendingUpdate(PowerLevel powerLevel, const UserSettings& newSettings,
                            uint32_t blockId, const TransactionId& transactionId)
{
    ASSERT(powerLevel != PowerLevel::INVALID);

    uint32_t executionDelay = settings.rules.keysUpdateTimes[powerLevel.getIndex()];

    auto pendingUpdate = std::make_shared<UserUpdate>();
    pendingUpdate->blockId = blockId + executionDelay;
    pendingUpdate->settings = newSettings;
    pendingUpdate->transactionId = transactionId;
    pendingUpdate->powerLevel = powerLevel;
    this->pendingUpdate = pendingUpdate;
}

void User::executePendingUpdate(uint32_t blockId)
{
    ASSERT(!!pendingUpdate);
    ASSERT(blockId >= pendingUpdate->blockId);

    lastSettingsUpdate = blockId;
    settingsTransaction = pendingUpdate->transactionId;
    settings = pendingUpdate->settings;
    spendings = {};
    pendingUpdate = nullptr;
}

void User::serialize(Serializer& s)
{
    s(version);
    if (version != 1) {
        THROW_SERIALIZER_EXCEPTION("Invalid User version");
    }

    s(id);
    s(creator);
    s(iteration);
    s(committedIn);
    s(lastSettingsUpdate);

    // balances
    s(tokens);
    s(freeTransactions);

    // security
    s.serialize<uint8_t>(lockedKeys);
    s(logout);

    // mining
    s(miner);

    // statistics 
    s(spendings);
    s(operations);
    s(sponsors);

    // update
    s.serializeOptional(pendingUpdate);

    // settings
    s(settings);
    s(settingsTransaction);
}

void User::toJSON(json& j) const
{
    j["id"] = id;
    j["creator"] = creator;
    j["iteration"] = iteration;
    j["committed_in"] = committedIn;
    j["last_settings_update"] = lastSettingsUpdate;
    j["tokens"] = tokens;
    j["free_transactions"] = freeTransactions;
    j["locked_keys"] = lockedKeys;
    j["logout"] = logout;
    j["miner"] = miner;
    j["pending_update"] = pendingUpdate;
    j["operations"] = operations;
    j["sponsors"] = sponsors;
    j["spendings"] = spendings;
    j["settings"] = settings;
}

}
