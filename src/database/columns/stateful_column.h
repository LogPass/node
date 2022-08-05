#pragma once

#include "column.h"

namespace logpass {
namespace database {

struct ColumnState {
    uint8_t version = 1;
    uint32_t blockId = 0;

    virtual void serialize(Serializer& s)
    {
        s(version);
        s(blockId);
    }
};

// base class for columns with state
template<class State>
class StatefulColumn : public Column {
    static_assert(std::is_base_of_v<ColumnState, State>, "State must derive from ColumnState");

public:
    using Column::Column;
    virtual ~StatefulColumn() = default;

    // loads column state from database, should be used when column is initialized or rollbacked
    // m_mutex unique lock must be aquired when calling
    virtual void load() override
    {
        auto s = get(rocksdb::Slice());
        if (!s)
            return;
        m_confirmedState = State(); // clears current state
        (*s)(m_confirmedState);
        m_state = m_confirmedState;
    }

    // prepares commit, write changes to write batch, there's no need for lock
    virtual void prepare(uint32_t blockId, rocksdb::WriteBatch& batch) override
    {
        m_state.blockId = blockId;
        Serializer s;
        s(m_state);
        batch.Put(m_handle, rocksdb::Slice(), s);
    }

    // commits changes, m_mutex unique lock must be aquired when calling
    virtual void commit() override
    {
        m_confirmedState = m_state;
    }

    // clears temporary values, m_mutex unique lock must be aquired when calling
    virtual void clear() override
    {
        m_state = m_confirmedState;
    }

protected:
    State& state(bool confirmed = false)
    {
        return confirmed ? m_confirmedState : m_state;
    }

    const State& state(bool confirmed = false) const
    {
        return confirmed ? m_confirmedState : m_state;
    }

private:
    State m_state, m_confirmedState;
};

}
}
