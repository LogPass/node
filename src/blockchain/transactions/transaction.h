#pragma once

#include <models/user/power_level.h>
#include <models/user/user_supervisor_settings.h>

namespace logpass {

class Transaction;
using Transaction_ptr = std::shared_ptr<Transaction>;
using Transaction_cptr = std::shared_ptr<const Transaction>;

struct UnconfirmedDatabase;

class TransactionValidationError : public Exception
{
    using Exception::Exception;
};

#define THROW_TRANSACTION_EXCEPTION(message) THROW_EXCEPTION(TransactionValidationError(message))

struct TransactionSettings {
    bool ignoresLock = false;
    bool isBlockchainManagementTransaction = false;
    bool isUserManagementTransaction = false;
    uint64_t transactionFeeMultiplier = 1;
    PowerLevel minimumPowerLevel = PowerLevel::LOW;
};

class Transaction : public std::enable_shared_from_this<Transaction>
{
    friend class std::shared_ptr<Transaction>;
    static constexpr std::string_view SIGNATURE_PREFIX = "LOGPASS SIGNED TRANSACTION:\n";

protected:
    explicit Transaction(uint8_t type);

public:
    Transaction(const Transaction& u) = delete;
    Transaction& operator= (const Transaction& u) = delete;

    virtual ~Transaction() = default;

    static Transaction_cptr load(Serializer& s);

    // validates signatures
    bool validateSignatures() const;

    // prepare data to preload to execute transaction faster
    virtual void preload(uint32_t blockId, UnconfirmedDatabase& database) const noexcept;

    // throws exception TransactionValidationError if not valid
    virtual void validate(uint32_t blockId, const UnconfirmedDatabase& database) const;
    
    // can't throw, transaction must be validated before execution
    virtual void execute(uint32_t blockId, UnconfirmedDatabase& database) const noexcept;

    // returns fee of transaction (fee can be converted to stake, cost can not, it is affected by pricing)
    virtual uint64_t getFee() const noexcept;

    // return cost of transaction (cost can not be converted to stake, it is not affected by pricing)
    virtual uint64_t getCost() const noexcept;

    // serializes transaction
    void serialize(Serializer& s);

    // seralizes to json
    virtual void toJSON(json& j) const;

    constexpr virtual TransactionSettings getTransactionSettings() const
    {
        return TransactionSettings();
    }

    // simple getters
    Hash getDuplicationHash() const
    {
        Serializer duplicationHash;
        duplicationHash(m_hash);
        duplicationHash.put<PublicKey>(m_signatures.getPublicKey());
        duplicationHash.put<UserId>(m_signatures.getUserId());
        return Hash(duplicationHash);
    }

    uint8_t getType() const
    {
        return m_type;
    }

    uint32_t getBlockId() const
    {
        return m_blockId;
    }

    int16_t getPricing() const
    {
        return m_pricing;
    }

    const UserId& getUserId() const
    {
        return m_signatures.getUserId();
    }

    uint32_t getSize() const
    {
        return m_id.getSize();
    }

    const TransactionId& getId() const
    {
        return m_id;
    }

    const PublicKey& getMainKey() const
    {
        return m_signatures.getPublicKey();
    }

    // operations
    Transaction_ptr setPublicKey(const PublicKey& publicKey)
    {
        m_signatures.setPublicKey(publicKey);
        return shared_from_this();
    }

    Transaction_ptr setUserId(const UserId& userId)
    {
        m_signatures.setUserId(userId);
        return shared_from_this();
    }

    Transaction_ptr setSponsorId(const UserId& supervisorId)
    {
        m_signatures.setSponsorId(supervisorId);
        return shared_from_this();
    }

    Transaction_ptr sign(const std::vector<PrivateKey>& keys)
    {
        m_signatures.sign(SIGNATURE_PREFIX, m_hash, keys);
        reload();
        return shared_from_this();
    }

    Transaction_ptr sign(const PrivateKey& key)
    {
        return sign(std::vector<PrivateKey>{ key });
    }

protected:
    // serialize custom fields of transaction
    virtual void serializeBody(Serializer& s) = 0;

    void reload();

protected:
    // header
    uint8_t m_type = 0;
    uint32_t m_blockId = 0;
    int16_t m_pricing = 0;

    // footer
    MultiSignatures m_signatures;

    // auto calculated, cached values
    Hash m_hash;
    TransactionId m_id;

#ifndef NDEBUG
    // debugging
    mutable std::atomic<uint32_t> m_validated = 0;
#endif
};

}
