#pragma once

#include "transaction.h"

namespace logpass {

class SponsorUserTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x04;
    SponsorUserTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const UserId& userId,
                                  uint8_t sponsoredTransactions, const Hash& sponsor = Hash());

    // prepare data to preload to execute transaction faster
    void preload(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    uint64_t getFee() const noexcept override;

    void toJSON(json& j) const final;

protected:
    void serializeBody(Serializer& s) override;

private:
    UserId m_userId;
    uint8_t m_sponsoredTransactions = 0;
    Hash m_sponsor;
};

}
