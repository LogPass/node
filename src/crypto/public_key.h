#pragma once

#include "crypto_array.h"
#include "hash.h"
#include "signature.h"

namespace logpass {

enum class PublicKeyTypes : uint8_t {
    INVALID = 0x00,
    ED25519 = 0x01
};

class PublicKey : public CryptoArray<33> {
    friend class PrivateKey;
public:
    using CryptoArray::CryptoArray;
    virtual ~PublicKey() = default;

    static PublicKey generateRandom();

    void serialize(Serializer& s)
    {
        if (s.peek<uint8_t>() != (uint8_t)PublicKeyTypes::ED25519) {
            THROW_SERIALIZER_EXCEPTION("Invalid PublicKey type");
        }
        s(*this);
    }

    void serialize(Serializer& s) const
    {
        s(*this);
    }

    void serializeKey(uint8_t type, Serializer& s)
    {
        if (type != (uint8_t)PublicKeyTypes::ED25519) {
            THROW_SERIALIZER_EXCEPTION("Invalid PublicKey type");
        }

        s.serialize(*this, 1);
        at(0) = type;
    }

    void serializeKey(Serializer& s) const
    {
        s.serialize(*this, 1);
    }

    uint8_t getType() const
    {
        return at(0);
    }

    bool verify(std::string_view prefix, const Serializer& s, const Signature& signature) const;
    bool verify(std::string_view prefix, const uint8_t* data, size_t size, const Signature& signature) const;
};

}
