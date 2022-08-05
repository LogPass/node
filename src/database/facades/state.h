#pragma once

#include "facade.h"

#include <database/columns/default.h>

namespace logpass {
namespace database {

class StateFacade : public Facade {
public:
    StateFacade(DefaultColumn* defaultColumn, bool confirmed) : m_default(defaultColumn), m_confirmed(confirmed) {};

    StateFacade(const std::unique_ptr<DefaultColumn>& defaultColumn, bool confirmed) :
        StateFacade(defaultColumn.get(), confirmed)
    {}

    void setVersion(uint16_t version);
    uint16_t getVersion() const;

    void setPricing(int16_t pricing);
    int16_t getPricing() const;

private:
    DefaultColumn* m_default;
    bool m_confirmed;
};

}
}
