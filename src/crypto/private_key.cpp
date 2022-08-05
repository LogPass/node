#include "pch.h"
#include "private_key.h"
#include "exception.h"

namespace logpass {

PrivateKey::PrivateKey(EVP_PKEY* pkey)
{
    m_pkey = std::shared_ptr<EVP_PKEY>(pkey, [](EVP_PKEY* pkey) {
        EVP_PKEY_free(pkey);
    });

    m_publicKey.at(0) = 0x01;
    size_t len = m_publicKey.size() - 1;
    int ret = EVP_PKEY_get_raw_public_key(m_pkey.get(), m_publicKey.data() + 1, &len);
    if (ret != 1) {
        THROW_EXCEPTION(CryptoException("EVP_PKEY_get_raw_public_key returned: "s + std::to_string(ret)));
    }
}

PrivateKey::PrivateKey(const std::shared_ptr<EVP_PKEY>& pkey, const PublicKey& publicKey)
    : m_pkey(pkey), m_publicKey(publicKey)
{
}

PrivateKey PrivateKey::fromPEM(const std::string_view data, const std::string_view password)
{
    BIO* buffer = BIO_new_mem_buf(data.data(), data.size());
    if (!buffer) {
        THROW_EXCEPTION(CryptoException("BIO_new_mem_buf failed"s));
    }
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(buffer, NULL, [](char* buf, int size, int rwflag, void* u) -> int {
        const std::string_view* password = (const std::string_view*)u;
        if (password->empty()) {
            return 0;
        }
        if (password->size() > size) {
            return -1;
        }
        memcpy(buf, password->data(), password->size());
        return password->size();
    }, (void*)&password);
    BIO_free(buffer);
    if (pkey == nullptr) {
        THROW_EXCEPTION(CryptoException("PEM_read_bio_PrivateKey failed"s));
    }
    if (EVP_PKEY_base_id(pkey) != EVP_PKEY_ED25519) {
        EVP_PKEY_free(pkey);
        THROW_EXCEPTION(CryptoException("Loaded PrivateKey is not ed25519 key"s));
    }

    return PrivateKey(pkey);
}

PrivateKey PrivateKey::generate()
{
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (!pctx) {
        THROW_EXCEPTION(CryptoException("EVP_PKEY_CTX_new_id failed"s));
    }
    int ret = EVP_PKEY_keygen_init(pctx);
    if (ret != 1) {
        THROW_EXCEPTION(CryptoException("EVP_PKEY_keygen failed"s));
    }
    ret = EVP_PKEY_keygen(pctx, &pkey);
    if (ret != 1 || !pkey) {
        THROW_EXCEPTION(CryptoException("EVP_PKEY_keygen failed"s));
    }
    EVP_PKEY_CTX_free(pctx);

    return PrivateKey(pkey);
}

std::vector<PrivateKey> PrivateKey::generate(size_t count)
{
    std::vector<PrivateKey> ret;
    for (size_t i = 0; i < count; ++i) {
        ret.push_back(PrivateKey::generate());
    }
    return ret;
}

Signature PrivateKey::sign(std::string_view prefix, const Serializer& serializer) const
{
    return sign(prefix, serializer.begin(), serializer.size());
}

Signature PrivateKey::sign(std::string_view prefix, const uint8_t* data, size_t size) const
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        THROW_EXCEPTION(CryptoException("EVP_MD_CTX_new failed"s));
    }
    int ret = EVP_DigestSignInit(ctx, NULL, NULL, NULL, m_pkey.get());
    if (ret != 1) {
        THROW_EXCEPTION(CryptoException("EVP_DigestSignInit failed"s));
    }
    std::vector<uint8_t> prefixedData(prefix.begin(), prefix.end());
    prefixedData.reserve(prefix.size() + size);
    std::copy_n(data, size, std::back_inserter(prefixedData));
    Signature signature;
    size_t len = signature.size();
    ret = EVP_DigestSign(ctx, signature.data(), &len, prefixedData.data(), prefixedData.size());
    if (ret != 1 || len != signature.size()) {
        THROW_EXCEPTION(CryptoException("EVP_DigestSignInit failed"s));
    }
    EVP_MD_CTX_free(ctx);
    return signature;
}

}
