#pragma once

namespace logpass {

template<int T>
class CryptoArray : protected std::array<uint8_t, T>
{
public:
    // size in bytes
    static constexpr size_t SIZE = T;

    CryptoArray()
    {
        std::fill(this->begin(), this->end(), 0);
    }

    CryptoArray(std::string_view str)
    {
        if (base64url_unpadded::decoded_max_size(str.size()) >= SIZE && base64url_unpadded::decoded_max_size(str.size()) <= SIZE + 2) {
            try {
                std::vector<uint8_t> raw = base64url_unpadded::decode(str);
                if (raw.size() == SIZE) {
                    for (size_t i = 0; i < SIZE; ++i) {
                        this->at(i) = raw[i];
                    }
                    return;
                }
            } catch (cppcodec::parse_error&) {}
        }

        if (base64::decoded_max_size(str.size()) >= SIZE && base64::decoded_max_size(str.size()) <= SIZE + 2) {
            try {
                std::vector<uint8_t> raw = base64::decode(str);
                if (raw.size() == SIZE) {
                    for (size_t i = 0; i < SIZE; ++i) {
                        this->at(i) = raw[i];
                    }
                    return;
                }
            } catch (cppcodec::parse_error&) {}
        }

        std::fill(this->begin(), this->end(), 0);
    }

    explicit CryptoArray(const uint8_t* data, size_t size)
    {
        ASSERT(size == SIZE);
        std::copy_n(data, size, begin());
    }


    virtual ~CryptoArray() = default;

    operator const rocksdb::Slice() const
    {
        return rocksdb::Slice((const char*)data(), size());
    }

    bool isValid() const
    {
        for (const uint8_t& v : *this) {
            if (v != 0) {
                return true;
            }
        }
        return false;
    }

    using std::array<uint8_t, T>::data;
    using std::array<uint8_t, T>::size;
    using std::array<uint8_t, T>::begin;
    using std::array<uint8_t, T>::end;
    using std::array<uint8_t, T>::cbegin;
    using std::array<uint8_t, T>::cend;

    std::string toString() const
    {
        return toBase64();
    }

    std::string toBase64() const
    {
        return base64url::encode(this->data(), this->size());
    }

    auto operator<=>(const CryptoArray<T>&) const = default;

    friend std::ostream& operator << (std::ostream& out, const CryptoArray& arr)
    {
        out << arr.toString();
        return out;
    }

    void toJSON(json& j) const
    {
        if (isValid()) {
            j = toString();
        } else {
            j = json::value_t::null;
        }
    }
};

template<int T>
inline void to_json(json& j, const CryptoArray<T>& c)
{
    if (c.isValid()) {
        j = c.toString();
    } else {
        j = json::value_t::null;
    }
}

template <class U>
concept is_cryptoarray = std::is_base_of<CryptoArray<U::SIZE>, U>::value;

template <class T> requires is_cryptoarray<T>
inline void from_json(const json& j, T& c)
{
    if (j.is_string()) {
        c = T(j.get<std::string>());
    } else {
        c = T();
    }
}

}
