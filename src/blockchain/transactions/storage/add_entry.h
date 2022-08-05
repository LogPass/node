#pragma once

#include "../transaction.h"

namespace logpass {

class StorageAddEntryTransaction : public Transaction {
public:
    constexpr static uint8_t TYPE = 0x55;
    StorageAddEntryTransaction() : Transaction(TYPE) {}

    static Transaction_ptr create(uint32_t blockId, int16_t pricing, const std::string& prefix, const std::string& key,
                                  const std::string& value);

    // throws exception TransactionValidationError if not valid
    void validate(uint32_t blockId, const UnconfirmedDatabase& database) const override;

    // can't throw, transaction must be validated before execution
    void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept override;

    // returns cost of transaction
    virtual uint64_t getFee() const noexcept override;

    void toJSON(json& j) const final;

    const std::string& getPrefix() const
    {
        return m_prefix;
    }

    const std::string& getKey() const
    {
        return m_key;
    }

    const std::string& getValue() const
    {
        return m_value;
    }

protected:
    void serializeBody(Serializer& s) override;

private:
    std::string m_prefix;
    std::string m_key;
    std::string m_value;
};

}
