#pragma once

#include "user_settings_validation_error.h"

namespace logpass {

struct UserSecurityRules {
    std::array<uint8_t, kUserPowerLevels> powerLevels = {};
    std::array<uint32_t, kUserPowerLevels> keysUpdateTimes = {};
    std::array<uint32_t, kUserPowerLevels> supervisorsUpdateTimes = {};
    std::array<uint32_t, kUserPowerLevels> rulesUpdateTimes = {};
    std::array<uint64_t, kUserPowerLevels> spendingLimits = {};

    uint8_t supervisingPowerLevel = 0; // minimum power level to act as supervisor

    void serialize(Serializer& s)
    {
        s(powerLevels);
        s(keysUpdateTimes);
        s(supervisorsUpdateTimes);
        s(rulesUpdateTimes);
        s(spendingLimits);
        s(supervisingPowerLevel);
    }

    void validate() const
    {
        // check if order is correct, higher power level should have smaller or equal value (except powerLevels)
        for (size_t i = 1; i < kUserPowerLevels; ++i) {
            if (powerLevels[i - 1] > powerLevels[i]) {
                THROW_USER_SETTINGS_EXCEPTION("Invalid power levels");
            }
            if (keysUpdateTimes[i] > keysUpdateTimes[i - 1]) {
                THROW_USER_SETTINGS_EXCEPTION("Invalid keys update times");
            }
            if (rulesUpdateTimes[i] > rulesUpdateTimes[i - 1]) {
                THROW_USER_SETTINGS_EXCEPTION("Invalid rules update times");
            }
            if (supervisorsUpdateTimes[i] > supervisorsUpdateTimes[i - 1]) {
                THROW_USER_SETTINGS_EXCEPTION("Invalid supervisor update times");
            }
            if (spendingLimits[i] < spendingLimits[i - 1]) {
                THROW_USER_SETTINGS_EXCEPTION("Invalid spending limits");
            }
        }

        for (auto& powerLevel : powerLevels) {
            if (powerLevel > kMaxPower) {
                THROW_USER_SETTINGS_EXCEPTION("Invalid power level (too high)");
            }
        }

        for (auto& keysUpdateTime : keysUpdateTimes) {
            if (keysUpdateTime > kUserMaxUpdateDelay) {
                THROW_USER_SETTINGS_EXCEPTION("Too high keys update time");
            }
        }
        for (auto& supervisorsUpdateTime : supervisorsUpdateTimes) {
            if (supervisorsUpdateTime > kUserMaxUpdateDelay) {
                THROW_USER_SETTINGS_EXCEPTION("Too high supervisors update time");
            }
        }
        for (auto& rulesUpdateTime : rulesUpdateTimes) {
            if (rulesUpdateTime > kUserMaxUpdateDelay) {
                THROW_USER_SETTINGS_EXCEPTION("Too high rules update time");
            }
        }

        if (supervisingPowerLevel >= kUserPowerLevels) {
            THROW_USER_SETTINGS_EXCEPTION("Invalid supervising power level");
        }
    }

    bool operator==(const UserSecurityRules& second) const = default;

    void toJSON(json& j) const
    {
        j["power_levels"] = powerLevels;
        j["keys_update_times"] = keysUpdateTimes;
        j["supervisors_update_times"] = supervisorsUpdateTimes;
        j["rules_update_times"] = rulesUpdateTimes;
        j["spending_limits"] = spendingLimits;
        j["supervising_power_level"] = supervisingPowerLevel;
    }
};

}
