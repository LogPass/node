#pragma once

#include "private_key.h"

namespace logpass {

class Certificate {
public:
    Certificate(const PrivateKey& privateKey);
    ~Certificate();

    boost::asio::ssl::context& getContext()
    {
        return m_context;
    }

private:
    boost::asio::ssl::context m_context;
    std::shared_ptr<EVP_PKEY> m_pkey;
    X509* m_x509;
};

}
