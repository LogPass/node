#pragma once

#include "transaction.h"

namespace logpass {

class WithdrawStakeTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x28;
    WithdrawStakeTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const MinerId& minerId, uint64_t unlockedStake,
                                  uint64_t lockedStake);

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    void toJSON(json& j) const final;

    constexpr virtual TransactionSettings getTransactionSettings() const
    {
        return TransactionSettings{
            .minimumPowerLevel = PowerLevel::MEDIUM
        };
    }

protected:
    void serializeBody(Serializer& s) override;

private:
    MinerId m_minerId;
    uint64_t m_unlockedStake = 0;
    uint64_t m_lockedStake = 0;
};

}
