#pragma once

#include "../transaction.h"
#include <models/storage/prefix_settings.h>

namespace logpass {

class StorageUpdatePrefixTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x51;
    StorageUpdatePrefixTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const std::string& prefix,
                                  const PrefixSettings& settings);

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
    std::string m_prefix;
    PrefixSettings m_settings;
};

}
