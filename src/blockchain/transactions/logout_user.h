#pragma once

#include "transaction.h"

namespace logpass {

class LogoutUserTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x0E;
    LogoutUserTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing);

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

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

};

}
