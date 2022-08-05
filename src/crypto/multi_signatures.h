#pragma once

#include "hash.h"
#include "public_key.h"
#include "private_key.h"
#include "signature.h"

namespace logpass {

enum class MultiSignaturesTypes : uint8_t {
    INVALID = 0x00,
    USER = 0x10,
    SPONSOR = 0x20
};

class MultiSignatures {
public:
    MultiSignatures() = default;

    void serialize(Serializer& s)
    {
        s(m_type);
        m_publicKey.serializeKey(m_type & 0x0F, s);
        if (getType() == MultiSignaturesTypes::USER) {
            s(m_userId);
        } else if (getType() == MultiSignaturesTypes::SPONSOR) {
            s(m_userId);
            s(m_sponsorId);
        }
        s.serialize<uint8_t>(m_signatures);
        s(m_signature);
        if (!validate()) {
            THROW_SERIALIZER_EXCEPTION("Invalid MultiSignature scheme");
        }
    }

    void serialize(Serializer& s, bool withSignatures = true) const
    {
        s(m_type);
        m_publicKey.serializeKey(s);
        if (getType() == MultiSignaturesTypes::USER) {
            s(m_userId);
        } else if (getType() == MultiSignaturesTypes::SPONSOR) {
            s(m_userId);
            s(m_sponsorId);
        }
        if (withSignatures) {
            s.serialize<uint8_t>(m_signatures);
            s(m_signature);
        }
    }

    bool validate() const
    {
        if (getType() != MultiSignaturesTypes::USER && getType() != MultiSignaturesTypes::SPONSOR) {
            return false;
        }
        if (getKeyType() != PublicKeyTypes::ED25519) {
            return false;
        }
        if (m_signatures.contains(m_publicKey)) {
            return false;
        }
        if (m_signatures.size() > 10) {
            return false;
        }
        if (m_userId == m_sponsorId) {
            return false;
        }
        return true;
    }

    bool verify(std::string_view prefix, const Hash& hash) const
    {
        if (!validate()) {
            return false;
        }

        Serializer s;
        s(hash);
        serialize(s, false);

        for (auto& [publicKey, signature] : m_signatures) {
            if (!publicKey.verify(prefix, s, signature)) {
                return false;
            }
        }
        s.serialize<uint8_t>(m_signatures);
        return m_publicKey.verify(prefix, s, m_signature);
    }

    MultiSignaturesTypes getType() const
    {
        switch (m_type & 0xF0) {
        case (uint8_t)MultiSignaturesTypes::USER:
            return MultiSignaturesTypes::USER;
        case (uint8_t)MultiSignaturesTypes::SPONSOR:
            return MultiSignaturesTypes::SPONSOR;
        }
        return MultiSignaturesTypes::INVALID;
    }

    PublicKeyTypes getKeyType() const
    {
        switch (m_type & 0x0F) {
        case (uint8_t)PublicKeyTypes::ED25519:
            return PublicKeyTypes::ED25519;
        }
        return PublicKeyTypes::INVALID;
    }

    const PublicKey& getPublicKey() const
    {
        return m_publicKey;
    }

    const UserId& getUserId() const
    {
        return m_userId;
    }

    const UserId& getSponsorId() const
    {
        return m_sponsorId;
    }

    size_t getSize() const
    {
        return m_signatures.size() + 1;
    }

    bool contains(const PublicKey& key) const
    {
        if (m_publicKey == key) {
            return true;
        }
        return m_signatures.contains(key);
    }

    void setPublicKey(const PublicKey& publicKey)
    {
        m_publicKey = publicKey;
        m_type &= 0xF0;
        m_type |= publicKey.getType();
    }

    void setUserId(const UserId& userId)
    {
        m_userId = userId;
        if (getType() == MultiSignaturesTypes::INVALID) {
            m_type &= 0x0F;
            m_type |= (uint8_t)MultiSignaturesTypes::USER;
        }
    }

    void setSponsorId(const UserId& sponsorId)
    {
        if (m_sponsorId == sponsorId) {
            return;
        }
        m_sponsorId = sponsorId;
        if (getType() != MultiSignaturesTypes::SPONSOR) {
            m_type &= 0x0F;
            m_type |= (uint8_t)MultiSignaturesTypes::SPONSOR;
        }
    }

    void sign(std::string_view prefix, const Hash& hash, const std::vector<PrivateKey>& keys)
    {
        ASSERT(m_userId.isValid());
        if (!m_publicKey.isValid() && !keys.empty()) {
            setPublicKey(keys.front().publicKey());
        }

        Serializer s;
        s(hash);
        serialize(s, false);

        PrivateKey finalKey;
        for (auto& key : keys) {
            if (m_publicKey == key.publicKey()) {
                finalKey = key;
                continue;
            }
            m_signatures.emplace(key.publicKey(), key.sign(prefix, s));
        }
        if (!finalKey.isValid()) {
            return;
        }

        s.serialize<uint8_t>(m_signatures);
        m_signature = finalKey.sign(prefix, s);
    }

protected:
    uint8_t m_type = 0x00;
    PublicKey m_publicKey;
    UserId m_userId;
    UserId m_sponsorId;
    std::map<PublicKey, Signature> m_signatures;
    Signature m_signature;
};

}
