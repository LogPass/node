#pragma once

#include "block/block.h"
#include "transactions/transaction.h"

namespace logpass {

class EventsListener;

class Events : public EventLoopThread {
    friend class EventsListener;
    friend class SharedThread<Events>;

public:
    Events() : EventLoopThread("events")
    {}

    void onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch);
    void onNewTransactions(const std::vector<Transaction_cptr>& transactions);

protected:
    void registerListener(EventsListener* listener);
    void unregisterListener(EventsListener* listener);

private:
    std::vector<EventsListener*> m_listeners;
};

struct EventsListenerCallbacks {
    const std::function<void(std::vector<Block_cptr>, bool)> onBlocks;
    const std::function<void(std::vector<Transaction_cptr>)> onNewTransactions;
};

class [[nodiscard]] EventsListener {
public:
    EventsListener(const std::weak_ptr<Events>& events, const EventsListenerCallbacks&& callbacks) :
        m_events(events), m_callbacks(std::move(callbacks))
    {
        if (auto events = m_events.lock()) {
            events->registerListener(this);
        }
    }

    ~EventsListener()
    {
        if (auto events = m_events.lock()) {
            events->unregisterListener(this);
        }
    }

    EventsListener(const EventsListener&) = delete;
    EventsListener& operator = (const EventsListener&) = delete;

    void onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch) const
    {
        if (m_callbacks.onBlocks) {
            m_callbacks.onBlocks(blocks, didChangeBranch);
        }
    }

    void onPendingTransaction(const std::vector<Transaction_cptr>& transaction) const
    {
        if (m_callbacks.onNewTransactions) {
            m_callbacks.onNewTransactions(transaction);
        }
    }

private:
    const std::weak_ptr<Events> m_events;
    const EventsListenerCallbacks m_callbacks;
};

}
