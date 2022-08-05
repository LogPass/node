#pragma once

#include "transaction.h"
#include <models/user/user_settings.h>

namespace logpass {

class UpdateUserTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x06;
    UpdateUserTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const UserSettings& settings);

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    void toJSON(json& j) const final;

    constexpr virtual TransactionSettings getTransactionSettings() const
    {
        return TransactionSettings{
            .ignoresLock = true,
            .isUserManagementTransaction = true,
            .minimumPowerLevel = PowerLevel::LOWEST
        };
    }

protected:
    void serializeBody(Serializer& s) override;

private:
    UserSettings m_settings;
};

}
