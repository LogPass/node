#pragma once

#include "transaction.h"

namespace logpass {

class IncreaseStakeTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x27;
    IncreaseStakeTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const MinerId& minerId, uint64_t value);

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    uint64_t getCost() const noexcept override;

    void toJSON(json& j) const final;

protected:
    void serializeBody(Serializer& s) override;

private:
    MinerId m_minerId;
    uint64_t m_value = 0;
};

}
