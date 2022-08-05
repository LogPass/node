#include "pch.h"
#include "state.h"

namespace logpass {
namespace database {

void StateFacade::setVersion(uint16_t version)
{
    m_default->setVersion(version);
}

uint16_t StateFacade::getVersion() const
{
    return m_default->getVersion(m_confirmed);
}

void StateFacade::setPricing(int16_t pricing)
{
    m_default->setPricing(pricing);
}

int16_t StateFacade::getPricing() const
{
    return m_default->getPricing(m_confirmed);
}

}
}
