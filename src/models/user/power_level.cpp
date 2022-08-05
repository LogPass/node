#include "pch.h"
#include "power_level.h"

namespace logpass {

PowerLevel::PowerLevel(uint8_t level, uint8_t power, uint8_t participants) :
    m_level(level), m_power(power), m_participants(participants)
{
    ASSERT(m_level < kUserPowerLevels);
    ASSERT(m_power <= kMaxPower);
    ASSERT(m_participants <= kUserMaxKeys + kUserMaxSupervisors);
    ASSERT(m_level == 0 || (m_power > 0 && m_participants > 0));
}

void PowerLevel::serialize(Serializer& s)
{
    s(m_level);
    s(m_power);
    s(m_participants);
}

void PowerLevel::serialize(Serializer& s) const
{
    s(m_level);
    s(m_power);
    s(m_participants);
}

uint8_t PowerLevel::getIndex() const
{
    return m_level;
}

}
