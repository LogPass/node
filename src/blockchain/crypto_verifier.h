#pragma once

#include <blockchain/transactions/transaction.h>

namespace logpass {

using TransactionVerifyCallback = SafeCallback<bool>;

class CryptoVerifier {
public:
    CryptoVerifier(size_t threads);
    ~CryptoVerifier();
    void stop();

    void verify(const Transaction_cptr& transaction, TransactionVerifyCallback&& callback);
    // verifies multiple transactions, blocks till done
    std::vector<uint8_t> verify(const std::vector<Transaction_cptr>& transactions);

private:
    asio::io_context m_context;
    asio::executor_work_guard<asio::io_context::executor_type> m_guard;
    std::vector<std::thread> m_threads;
    std::mutex m_mutex;
    std::atomic<bool> m_stopped = false;
};

}
