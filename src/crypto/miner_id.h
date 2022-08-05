#pragma once

#include "crypto_array.h"
#include "public_key.h"
#include "exception.h"

namespace logpass {

class MinerId : public CryptoArray<32> {
public:
    using CryptoArray::CryptoArray;

    MinerId(const PublicKey& publicKey) : CryptoArray(publicKey.data() + 1, publicKey.size() - 1)
    {
        if (publicKey.getType() != (uint8_t)PublicKeyTypes::ED25519) {
            THROW_EXCEPTION(CryptoException("Invalid PublicKey type to create MinerId"));
        }
    }

    MinerId& operator=(const PublicKey& publicKey)
    {
        if (publicKey.getType() != (uint8_t)PublicKeyTypes::ED25519) {
            THROW_EXCEPTION(CryptoException("Invalid PublicKey type to create MinerId"));
        }
        std::copy_n(publicKey.data() + 1, size(), data());
        return *this;
    }

    operator const PublicKey() const
    {
        PublicKey key;
        key.data()[0] = (uint8_t)PublicKeyTypes::ED25519;
        std::copy_n(data(), size(), key.data() + 1);
        return key;
    }
};

}
