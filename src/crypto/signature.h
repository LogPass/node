#pragma once

#include "crypto_array.h"

namespace logpass {

class Signature : public CryptoArray<64> {
    friend class PrivateKey;
    friend class PublicKey;
public:
    using CryptoArray::CryptoArray;
    Signature(const std::string& s) : CryptoArray(s) {};
};

}
