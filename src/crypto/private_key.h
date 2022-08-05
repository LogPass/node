#pragma once

#include "crypto_array.h"
#include "public_key.h"
#include "hash.h"
#include "signature.h"

namespace logpass {

class PrivateKey {
    friend class Certificate;

protected:
    PrivateKey(EVP_PKEY* pkey);
    PrivateKey(const std::shared_ptr<EVP_PKEY>& pkey, const PublicKey& publicKey);

public:
    PrivateKey() = default;

    PrivateKey(const PrivateKey& key) : PrivateKey(key.m_pkey, key.m_publicKey)
    {}

    PrivateKey& operator = (const PrivateKey& key)
    {
        m_pkey = key.m_pkey;
        m_publicKey = key.m_publicKey;
        return *this;
    }

    PrivateKey(PrivateKey&& key) : PrivateKey(key.m_pkey, key.m_publicKey)
    {}

    // loads private key from PEM format
    static PrivateKey fromPEM(const std::string_view data, const std::string_view password);

    // generate random private key
    static PrivateKey generate();

    // generate given number of random private keys
    static std::vector<PrivateKey> generate(size_t count);

    bool isValid() const
    {
        return !!m_pkey;
    }

    PublicKey publicKey() const
    {
        return m_publicKey;
    }

    operator const PublicKey() const
    {
        return m_publicKey;
    }

    Signature sign(std::string_view prefix, const Serializer& serializer) const;
    Signature sign(std::string_view prefix, const uint8_t* data, size_t size) const;

    bool verify(std::string_view prefix, const Serializer& s, const Signature& signature) const
    {
        return m_publicKey.verify(prefix, s, signature);
    }

    bool verify(std::string_view prefix, const uint8_t* data, size_t size, const Signature& signature) const
    {
        return m_publicKey.verify(prefix, data, size, signature);
    }

private:
    std::shared_ptr<EVP_PKEY> m_pkey;
    PublicKey m_publicKey;
};

}
