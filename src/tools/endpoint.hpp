#pragma once

#include "serializer.hpp"

namespace logpass {

class Endpoint {
public:
    Endpoint() = default;

    Endpoint(const std::string& endpoint, uint16_t port) : m_address(endpoint), m_port(port)
    {
        ASSERT(m_address.size() <= 30);
    }

    static Endpoint fromString(const std::string& str)
    {
        size_t colonPos = str.find_last_of(':');
        if (colonPos == std::string::npos) {
            return Endpoint();
        }
        uint16_t port = 0;
        std::from_chars(str.data() + colonPos + 1, str.data() + str.size(), port);
        return Endpoint(str.substr(0, colonPos), port);
    }

    void serialize(Serializer& s)
    {
        s.serialize<uint8_t>(m_address);
        if (m_address.size() > 30) {
            THROW_SERIALIZER_EXCEPTION("Maximum endpoint adress length is 30");
        }
        s(m_port);
    }

    bool isValid() const
    {
        return !m_address.empty() && m_port != 0;
    }

    std::string toString() const
    {
        if (!isValid()) {
            return "";
        }
        return m_address + ":" + std::to_string(m_port);
    }

    std::string getAddress() const
    {
        return m_address;
    }

    uint16_t getPort() const
    {
        return m_port;
    }

    bool operator==(const Endpoint& second) const
    {
        return m_address == second.m_address && m_port == second.m_port;
    }

    bool operator!=(const Endpoint& second) const
    {
        return !(*this == second);
    }

    friend std::ostream& operator << (std::ostream& out, const Endpoint& endpoint)
    {
        out << endpoint.toString();
        return out;
    }

private:
    std::string m_address;
    uint16_t m_port = 0;
};

inline void to_json(json& j, const Endpoint& adr)
{
    if (adr.isValid()) {
        j = adr.toString();
    } else {
        j = json::value_t::null;
    }
}

inline void from_json(const json& j, Endpoint& c)
{
    c = Endpoint::fromString(j.get<std::string>());
}

}
