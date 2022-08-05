#pragma once

#include "transaction.h"

namespace logpass {

// special transaction, inits blockchain, creates first user and miner
class InitTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x01;
    InitTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, uint64_t initializationTime,
                                  uint32_t blockInterval);

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    constexpr virtual TransactionSettings getTransactionSettings() const
    {
        return TransactionSettings{
            .isBlockchainManagementTransaction = true
        };
    }

    void toJSON(json& j) const final;

    uint64_t getInitializationTime() const
    {
        return m_initializationTime;
    }

    uint32_t getBlockInterval() const
    {
        return m_blockInterval;
    }

protected:
    void serializeBody(Serializer& s) override;

private:
    uint8_t m_version = 1;
    uint64_t m_initializationTime = 0; // timestamp of blockchain initialization
    uint32_t m_blockInterval = 0;
};

}
