#pragma once

#include "serializer.hpp"

class IPAddress
{
public:
    IPAddress()
    {
        std::fill(m_address.begin(), m_address.end(), 0);
    }

    IPAddress(const std::string& address)
    {
        std::fill(m_address.begin(), m_address.end(), 0);
        boost::system::error_code ec;
        auto ip = boost::asio::ip::address::from_string(address, ec);
        if (!ec) {
            if (ip.is_v4()) {
                auto bytes = ip.to_v4().to_bytes();
                std::copy(bytes.begin(), bytes.end(), m_address.begin() + 12);
            } else if (ip.is_v6())
                m_address = ip.to_v6().to_bytes();
        }
    }

    void serialize(Serializer& s)
    {
        for (auto& v : m_address)
            s(v);
    }

    bool isValid() const
    {
        for (const uint8_t& v : m_address)
            if (v != 0)
                return true;
        return false;
    }

    bool isIPv4() const
    {
        for (size_t i = 0; i < 12; ++i)
            if (m_address[i] != 0)
                return false;
        return m_address[12] != 0;
    }

    bool isIPv6() const
    {
        return m_address[0] != 0;
    }

    std::string toIPv4() const
    {
        return boost::asio::ip::make_address_v4(std::array<uint8_t, 4>{ m_address[12] , m_address[13], m_address[14], m_address[15] }).to_string();
    }

    std::string toIPv6() const
    {
        return boost::asio::ip::make_address_v6(m_address).to_string();
    }

    bool operator==(const IPAddress& second) const
    {
        return m_address == second.m_address;
    }

    bool operator!=(const IPAddress& second) const
    {
        return m_address != second.m_address;
    }

    std::string toString() const
    {
        if (!isValid())
            return "";
        if (isIPv4())
            return toIPv4();
        return toIPv6();
    }

    friend std::ostream& operator << (std::ostream& out, const IPAddress& arr)
    {
        out << (arr.isValid() ? arr.toString() : "INVALID_ADDRESS");
        return out;
    }

private:
    std::array<uint8_t, 16> m_address;
};

inline void to_json(json& j, const IPAddress& adr)
{
    j = adr.toString();
}

