#pragma once

#include "hash.h"
#include "crypto_array.h"
#include "public_key.h"

namespace logpass {

class UserId : public CryptoArray<32> {
public:
    using CryptoArray::CryptoArray;

    UserId(const PublicKey& publicKey) : CryptoArray(Hash(publicKey.data(), publicKey.size())) {}
};

}
