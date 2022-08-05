#pragma once

namespace logpass {

class PowerLevel {
public:
    static constexpr uint8_t INVALID[3] = { 0, 0, 0 };
    static constexpr uint8_t LOWEST[3] = { 0, 1, 1 };
    static constexpr uint8_t LOW[3] = { 1, 1, 1 };
    static constexpr uint8_t MEDIUM[3] = { 2, 1, 1 };
    static constexpr uint8_t HIGH[3] = { 3, 1, 1 };
    static constexpr uint8_t HIGHEST[3] = { 4, 1, 1 };

    PowerLevel() = default;
    explicit PowerLevel(uint8_t level, uint8_t power = 1, uint8_t participants = 1);
    constexpr PowerLevel(const uint8_t params[3]) : m_level(params[0]), m_power(params[1]), m_participants(params[2]) {}
    void serialize(Serializer& s);
    void serialize(Serializer& s) const;

    uint8_t getIndex() const;

    auto operator<=>(const PowerLevel&) const = default;

protected:
    uint8_t m_level = 0;
    uint8_t m_power = 0;
    uint8_t m_participants = 0;
};

}
