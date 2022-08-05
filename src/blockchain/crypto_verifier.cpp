#include "pch.h"
#include "crypto_verifier.h"

namespace logpass {

CryptoVerifier::CryptoVerifier(size_t threads) : m_context(), m_guard(boost::asio::make_work_guard(m_context))
{
    ASSERT(threads > 0);
    for (size_t i = 0; i < threads; ++i) {
        m_threads.push_back(std::thread([&] {
            SET_THREAD_NAME("verifier");
            m_context.run();
        }));
    }
}

CryptoVerifier::~CryptoVerifier()
{
    ASSERT(m_stopped);
    ASSERT(m_threads.empty());
}

void CryptoVerifier::stop()
{
    m_guard.reset();
    for (auto& thread : m_threads) {
        thread.join();
    }
    m_threads.clear();
    m_stopped = true;
}

void CryptoVerifier::verify(const Transaction_cptr& transaction, TransactionVerifyCallback&& callback)
{
    boost::asio::post(m_context, [this, transaction, callback = std::move(callback)]{
        callback(transaction->validateSignatures());
                      });
}

std::vector<uint8_t> CryptoVerifier::verify(const std::vector<Transaction_cptr>& transactions)
{
    if (transactions.size() == 0) {
        return {};
    }
    std::vector<uint8_t> results(transactions.size(), 0);
    std::promise<bool> promise;
    std::atomic<size_t> validatedTransactions = 0;
    for (size_t index = 0, lastIndex = transactions.size() - 1;  auto & tranasction : transactions) {
        boost::asio::post(m_context, [this, index, lastIndex, tranasction, &promise, &results, &validatedTransactions] {
            if (tranasction->validateSignatures()) {
                results[index] = 1;
            }
            if (validatedTransactions.fetch_add(1, std::memory_order_relaxed) == lastIndex) {
                promise.set_value(true);
            }
        });
        ++index;
    }
    promise.get_future().wait();
    return results;
}

}
