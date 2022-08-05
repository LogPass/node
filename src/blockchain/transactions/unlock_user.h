#pragma once

#include "transaction.h"

namespace logpass {

class UnlockUserTransaction : public Transaction
{
public:
    constexpr static uint8_t TYPE = 0x0B;
    UnlockUserTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const std::set<PublicKey>& keysToUnlock,
                                  const std::set<UserId>& supervisorsToUnlock);

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
            .minimumPowerLevel = PowerLevel::MEDIUM
        };
    }

protected:
    void serializeBody(Serializer& s) override;

private:
    std::set<PublicKey> m_keysToUnlock;
    std::set<UserId> m_supervisorsToUnlock;
};

}
