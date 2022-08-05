#pragma once

#include "transaction.h"

namespace logpass {

class TransferTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x10;
    TransferTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const UserId& destinationUser, uint64_t value);

    // prepare data to preload to execute transaction faster
    void preload(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    // returns fee of transaction
    virtual uint64_t getCost() const noexcept override;

    void toJSON(json& j) const final;

protected:
    void serializeBody(Serializer& s) override;

private:
    UserId m_destination;
    uint64_t m_value;
};

}
