#include "pch.h"
#include "public_key.h"

#include "exception.h"

namespace logpass {

PublicKey PublicKey::generateRandom()
{
    PublicKey pk;
    pk.data()[0] = (uint8_t)PublicKeyTypes::ED25519;
    int ret = RAND_bytes(pk.data() + 1, PublicKey::SIZE - 1);
    if (ret != 1) {
        THROW_EXCEPTION(CryptoException("RAND_bytes failed"s));
    }
    return pk;
}

bool PublicKey::verify(std::string_view prefix, const Serializer& s, const Signature& signature) const
{
    return verify(prefix, s.begin(), s.size(), signature);
}

bool PublicKey::verify(std::string_view prefix, const uint8_t* data, size_t size, const Signature& signature) const
{
    std::vector<uint8_t> prefixedData(prefix.begin(), prefix.end());
    prefixedData.reserve(prefix.size() + size);
    std::copy_n(data, size, std::back_inserter(prefixedData));
#ifdef USE_IROHA_ED25519
    signature_t sig = {};
    memcpy(sig.data, signature.data(), signature.size());
    public_key_t pub = {};
    memcpy(pub.data, this->data() + 1, this->size() - 1);
    int ret = ed25519_verify(&sig, prefixedData.data(), prefixedData.size(), &pub);
    return ret == 1;
#else
    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, this->data() + 1, this->size() - 1);
    if (!pkey) {
        THROW_EXCEPTION(CryptoException("EVP_PKEY_new_raw_public_key failed"s));
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        THROW_EXCEPTION(CryptoException("EVP_MD_CTX_new failed"s));
    }

    int ret = EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey);
    if (ret != 1) {
        THROW_EXCEPTION(CryptoException("EVP_DigestSignInit failed"s));
    }
    ret = EVP_DigestVerify(ctx, signature.data(), signature.size(), prefixedData.data(), prefixedData.size());
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ret == 1;
#endif
}

}
