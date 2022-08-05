#include "pch.h"
#include "certificate.h"
#include "exception.h"

namespace logpass {

Certificate::Certificate(const PrivateKey& privateKey) : m_context(boost::asio::ssl::context::tlsv13)
{
    m_pkey = privateKey.m_pkey;
    if (!m_pkey) {
        THROW_EXCEPTION(CryptoException("PrivateKey PKEY is null"));
    }

    m_x509 = X509_new();
    X509_set_version(m_x509, 2);

    ASN1_INTEGER_set(X509_get_serialNumber(m_x509), 1);
    X509_gmtime_adj(X509_get_notBefore(m_x509), 0);
    X509_gmtime_adj(X509_get_notAfter(m_x509), 31536000L);

    X509_set_pubkey(m_x509, m_pkey.get());
    X509_sign(m_x509, m_pkey.get(), nullptr);

    SSL_CTX_use_PrivateKey(m_context.native_handle(), m_pkey.get());
    SSL_CTX_use_certificate(m_context.native_handle(), m_x509);

    m_context.set_verify_mode(boost::asio::ssl::verify_peer | boost::asio::ssl::verify_fail_if_no_peer_cert);
}

Certificate::~Certificate()
{
    X509_free(m_x509);
}

}
