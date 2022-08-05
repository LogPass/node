#pragma once

#include "transaction.h"

namespace logpass {

class CommitTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x80;
    CommitTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const MinerId& minerId, uint32_t transactions,
                                  uint64_t users, uint64_t tokens, uint64_t stakedTokens);

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    void toJSON(json& j) const final;

    constexpr virtual TransactionSettings getTransactionSettings() const
    {
        return TransactionSettings{
            .isBlockchainManagementTransaction = true,
            .minimumPowerLevel = PowerLevel::LOWEST
        };
    }

    static uint64_t getMiningReward(int16_t pricing, uint32_t newTransactions, uint64_t users, uint64_t tokens,
                                    uint64_t stakedTokens);


protected:
    void serializeBody(Serializer& s) override;

private:
    uint8_t m_version = 1;
    MinerId m_minerId;
    uint32_t m_transactions = 0;
    uint64_t m_users = 0;
    uint64_t m_tokens = 0;
    uint64_t m_stakedTokens = 0;
    uint64_t m_reward = 0;
};

}
