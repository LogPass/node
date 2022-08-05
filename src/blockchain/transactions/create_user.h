#pragma once

#include "transaction.h"

namespace logpass {

class CreateUserTransaction : public Transaction
{
public:
    constexpr static uint8_t TYPE = 0x03;
    CreateUserTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const PublicKey& publicKey,
                                  uint8_t sponsoredTransactions, const Hash& sponsor = Hash());
    static Transaction_ptr create(const PublicKey& publicKey, uint32_t blockId)
    {
        return create(blockId, -1, publicKey, kUserMinFreeTransactions);
    };

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
    PublicKey m_publicKey;
    uint8_t m_sponsoredTransactions = 0;
    Hash m_sponsor;
};

}
