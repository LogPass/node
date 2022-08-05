#pragma once

#include "crypto_array.h"

namespace logpass {

class Hash : public CryptoArray<32> {
public:
    Hash() {};
    // generate Hash from serializer
    Hash(const Serializer& s);
    // load Hash from string
    Hash(const std::string& s) : CryptoArray(s) {};
    // load Hash from string_view
    Hash(const std::string_view& s) : CryptoArray(s) {};
    // generate Hash
    explicit Hash(const uint8_t* begin, const size_t size);
    // generate Hash from cryptoarray
    template <int T>
    explicit Hash(const CryptoArray<T>& arr) : Hash(arr.data(), arr.size()) {};

    virtual ~Hash() {}

    // generates hash from string
    static Hash generate(const std::string_view& s);

    // generates random hash
    static Hash generateRandom();
};

}
