#include "pch.h"
#include "events.h"

namespace logpass {

void Events::onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch)
{
    post([this, blocks, didChangeBranch] {
        std::shared_lock lock(m_mutex);
        for (auto& listener : m_listeners) {
            listener->onBlocks(blocks, didChangeBranch);
        }
    });
}

void Events::onNewTransactions(const std::vector<Transaction_cptr>& transactions)
{
    post([this, transactions] {
        std::shared_lock lock(m_mutex);
        for (auto& listener : m_listeners) {
            listener->onPendingTransaction(transactions);
        }
    });
}

void Events::registerListener(EventsListener* listener)
{
    std::unique_lock lock(m_mutex);
    m_listeners.push_back(listener);
}

void Events::unregisterListener(EventsListener* listener)
{
    std::unique_lock lock(m_mutex);
    auto it = std::remove(m_listeners.begin(), m_listeners.end(), listener);
    if (it != m_listeners.end()) {
        m_listeners.erase(it, m_listeners.end());
    }
}

}
