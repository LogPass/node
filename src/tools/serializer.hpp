#pragma once

#include "exception.hpp"

namespace logpass {

class Serializer;
using Serializer_ptr = std::shared_ptr<Serializer>;
using Serializer_cptr = std::shared_ptr<const Serializer>;

class SerializerException : public Exception {
    using Exception::Exception;
};

template<typename T> struct is_shared_ptr : std::false_type {};
template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template<typename T>
concept has_load = requires(T v)
{
    T::load;
};

template <class U>
concept is_uint8_array = std::is_base_of_v<std::array<uint8_t, U::SIZE>, U>;

#define THROW_SERIALIZER_EXCEPTION(message) THROW_EXCEPTION(SerializerException(message))

class Serializer {
public:
    static constexpr size_t MAX_SIZE = 40 * 1024 * 1024;
    static constexpr size_t COMPRESSION_LEVEL = 4;

    Serializer(uint32_t bufferSize = 1024)
    {
        bufferSize = std::max<uint32_t>(8, bufferSize);
        m_data.resize(bufferSize);
        m_reader = false;
    }

    Serializer(const std::vector<uint8_t>& data) : m_data(data)
    {
        m_reader = true;
    }

    Serializer(const std::vector<uint8_t>&& data) : m_data(std::move(data))
    {
        m_reader = true;
    }

    Serializer(const std::string& data) : m_data(std::vector<uint8_t>(data.begin(), data.end()))
    {
        m_reader = true;
    }

    Serializer(const rocksdb::Slice& slice) : m_data(slice.data(), slice.data() + slice.size())
    {
        m_reader = true;
    }

    Serializer(const char* data, size_t size) : m_data(data, data + size)
    {
        m_reader = true;
    }

    Serializer(std::istream& ifs, const std::string& file = "") : m_file(file)
    {
        m_reader = true;
        if (!ifs) {
            THROW_SERIALIZER_EXCEPTION("Invalid file (not opened)");
        }

        ifs.unsetf(std::ios_base::skipws);

        std::streampos size;
        ifs.seekg(0, std::ios::end);
        size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        if (size > MAX_SIZE) {
            THROW_SERIALIZER_EXCEPTION("File is too big");
        }

        m_data.reserve(size);
        m_data.insert(m_data.begin(), std::istream_iterator<uint8_t>(ifs), std::istream_iterator<uint8_t>());
    }

    Serializer(Serializer const&) = delete;
    Serializer& operator=(Serializer const&) = delete;

    Serializer(Serializer&& s) noexcept
    {
        m_reader = s.m_reader;
        m_data = std::move(s.m_data);
        m_pos = s.m_pos;
        m_file = std::move(s.m_file);
        s.m_data.clear();
        s.m_pos = 0;
    }

    // creates serializer from base64/base64url data
    static Serializer fromBase64(const std::string& data)
    {
        std::vector<uint8_t> decodedData;
        try {
            decodedData = base64url_unpadded::decode(data);
        } catch (cppcodec::parse_error&) {
            try {
                decodedData = base64::decode(data);
            } catch (cppcodec::parse_error&) {}
        }
        if (decodedData.empty()) {
            THROW_SERIALIZER_EXCEPTION("Invalid base64/base64url data");
        }
        return Serializer(std::move(decodedData));
    }

    // creates serializer from compressed data
    static Serializer fromCompressedData(Serializer& s)
    {
        if (!s.reader()) {
            THROW_SERIALIZER_EXCEPTION("Serializer can not read compressed data from writer");
        }

        unsigned long uncompressedDataSize = s.get<uint32_t>();
        if (uncompressedDataSize > MAX_SIZE) {
            THROW_SERIALIZER_EXCEPTION("Too big size of uncompressed data");
        }

        std::vector<uint8_t> compressedData;
        s.serialize<uint32_t>(compressedData);
        std::vector<uint8_t> uncompressedData(uncompressedDataSize);
        int ret = uncompress(uncompressedData.data(), &uncompressedDataSize, compressedData.data(),
                             compressedData.size());
        if (ret != Z_OK || uncompressedData.size() != uncompressedDataSize) {
            THROW_SERIALIZER_EXCEPTION("Can't decompress data");
        }
        return Serializer(std::move(uncompressedData));
    }

    operator const rocksdb::Slice() const
    {
        return rocksdb::Slice((const char*)buffer(), size());
    }

    operator const std::string_view() const
    {
        return std::string_view((const char*)buffer(), size());
    }

    void switchToReader(size_t size = 0)
    {
        if (m_reader) {
            THROW_SERIALIZER_EXCEPTION("Serializer is already a reader");
        }
        if (size > m_data.size()) {
            THROW_SERIALIZER_EXCEPTION("Invalid new size");
        }
        m_reader = true;
        m_data.resize(size == 0 ? m_pos : size);
        m_pos = 0;
    }

    void rewind()
    {
        if (!m_reader) {
            THROW_SERIALIZER_EXCEPTION("You can rewind only reader");
        }
        m_pos = 0;
    }

    // data is being read from serializer
    bool reader() const { return m_reader; }
    // new data is being written to serializer
    bool writer() const { return !m_reader; }
    size_t size() const { return reader() ? m_data.size() : m_pos; }
    size_t pos() const { return m_pos; }
    std::string file() const { return m_file; }
    bool eof() const { return reader() && pos() == size(); }

    uint8_t* buffer() { return m_data.data(); }
    const uint8_t* const buffer() const { return m_data.data(); }

    const uint8_t* const begin() const { return m_data.data(); }
    const uint8_t* const end() const { return m_data.data() + size(); }

    std::vector<uint8_t> dump() const { return std::vector<uint8_t>(begin(), end()); }

    template <typename T>
    T peek()
    {
        if (!reader()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::peek, can be used only by reader");
        }
        T ret = {};
        size_t pos = m_pos;
        serialize(ret);
        m_pos = pos;
        return ret;
    }

    template <typename T>
    T get()
    {
        if (!reader()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::get, can be used only by reader");
        }
        T ret = {};
        serialize(ret);
        return ret;
    }

    template <typename T>
    void put(T& value)
    {
        if (!writer()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::put, can be used only by writer");
        }
        serialize(value);
    }

    template <typename T>
    void put(const T& value)
    {
        if (!writer()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::put, can be used only by writer");
        }
        serialize(value);
    }

    template <typename T>
    void operator()(T& param)
    {
        serialize(param);
    }

    template <typename S, typename T>
    void operator()(T& param)
    {
        serialize<S>(param);
    }

    template <typename T>
    void serialize(T& param)
    {
        serializeBasic<T>(param);
    }

    // string view
    void readFrom(std::string_view sv)
    {
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't modify string_view");
        }

        write(sv.data(), sv.size());
    }

    // vector
    template <typename T>
    void serialize(std::vector<T>& v)
    {
        return serialize<uint16_t, T>(v);
    }

    // const vector
    template <typename T>
    void serialize(const std::vector<T>& v)
    {
        return serialize<uint16_t, T>(v);
    }

    // vector
    template <typename S, typename T>
    void serialize(std::vector<T>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            if (!v.empty()) {
                THROW_SERIALIZER_EXCEPTION("Vector is not empty");
            }
        } else {
            if (v.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("Vector has too many elements");
            }
        }
        S size = (S)v.size();
        serialize<S>(size);
        if (reader()) {
            v.resize(size);
        }
        for (S i = 0; i < size; ++i) {
            serialize(v[i]);
        }
    }

    // const vector
    template <typename S, typename T>
    void serialize(const std::vector<T>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't write to const vector");
        } else {
            if (v.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("Vector has too many elements");
            }
        }
        S size = (S)v.size();
        serialize<S>(size);
        for (S i = 0; i < size; ++i) {
            serialize(v[i]);
        }
    }

    // deque
    template <typename T>
    void serialize(std::deque<T>& v)
    {
        return serialize<uint16_t, T>(v);
    }

    template <typename S, typename T>
    void serialize(std::deque<T>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            if (!v.empty()) {
                THROW_SERIALIZER_EXCEPTION("Deque is not empty");
            }
        } else {
            if (v.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("Deque has too many elements");
            }
        }
        S size = (S)v.size();
        serialize<S>(size);
        if (reader()) {
            v.resize(size);
        }
        for (S i = 0; i < size; ++i) {
            serialize(v[i]);
        }
    }

    template <typename S, typename T>
    void serialize(const std::deque<T>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't write to const deque");
        } else {
            if (v.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("Deque has too many elements");
            }
        }
        S size = (S)v.size();
        serialize<S>(size);
        for (S i = 0; i < size; ++i) {
            serialize(v[i]);
        }
    }

    // string
    void serialize(std::string& v)
    {
        return serialize<uint16_t>(v);
    }

    // string
    template<typename S>
    void serialize(std::string& str)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            if (!str.empty()) {
                THROW_SERIALIZER_EXCEPTION("String is not empty");
            }
        } else {
            if (str.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("String has too many elements");
            }
        }

        S size = (S)str.size();
        serialize<S>(size);
        if (reader()) {
            if (m_pos + size > m_data.size()) {
                THROW_SERIALIZER_EXCEPTION("Serialzer::serialize string, pos + string_size > size");
            }
            str.resize(size);
            read(str.data(), size);
        } else {
            write(str.data(), size);
        }
    }

    // const string
    void serialize(const std::string& v)
    {
        return serialize<uint16_t>(v);
    }

    // const string
    template<typename S>
    void serialize(const std::string& str)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't write to const string");
        } else {
            if (str.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("String has too many elements");
            }
        }

        S size = (S)str.size();
        serialize<S>(size);
        write(str.data(), str.size());
    }

    // set
    template <typename T, typename C>
    void serialize(std::set<T, C>& v)
    {
        return serialize<uint16_t>(v);
    }

    // set
    template <typename S, typename T, typename C>
    void serialize(std::set<T, C>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            if (!v.empty()) {
                THROW_SERIALIZER_EXCEPTION("Set is not empty");
            }
        } else {
            if (v.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("Set has too many elements");
            }
        }
        S size = (S)v.size();
        serialize<S>(size);
        if (reader()) {
            for (S i = 0; i < size; ++i) {
                T value;
                serialize(value);
                auto op = v.insert(std::move(value));
                if (!op.second) {
                    THROW_SERIALIZER_EXCEPTION("Duplicated element in set");
                }
                if (++(op.first) != v.end()) {
                    THROW_SERIALIZER_EXCEPTION("Set elements are not sorted");
                }
            }
        } else {
            for (auto& it : v) {
                serialize(it);
            }
        }
    }

    // const set
    template <typename S, typename T, typename C>
    void serialize(const std::set<T, C>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't write to const set");
        } else if (v.size() > std::numeric_limits<S>::max()) {
            THROW_SERIALIZER_EXCEPTION("Set has too many elements");
        }
        S size = (S)v.size();
        serialize<S>(size);
        for (auto& it : v) {
            serialize(it);
        }
    }

    // map
    template <typename K, typename V>
    void serialize(std::map<K, V>& v)
    {
        return serialize<uint16_t, K, V>(v);
    }

    // const map
    template <typename K, typename V>
    void serialize(const std::map<K, V>& v)
    {
        return serialize<uint16_t, K, V>(v);
    }

    template <typename S, typename K, typename V>
    void serialize(std::map<K, V>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            if (!v.empty()) {
                THROW_SERIALIZER_EXCEPTION("Map is not empty");
            }
        } else {
            if (v.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("Map has too many elements");
            }
        }
        S size = (S)v.size();
        serialize<S>(size);
        if (reader()) {
            for (S i = 0; i < size; ++i) {
                std::pair<K, V> pair;
                serialize(pair);
                auto op = v.insert(std::move(pair));
                if (!op.second) {
                    THROW_SERIALIZER_EXCEPTION("Duplicated element in map");
                }
                if (++(op.first) != v.end()) {
                    THROW_SERIALIZER_EXCEPTION("Map elements are not sorted");
                }
            }
        } else {
            for (auto& [key, value] : v) {
                serialize(key);
                serialize(value);
            }
        }
    }

    template <typename S, typename K, typename V>
    void serialize(const std::map<K, V>& v)
    {
        static_assert(std::is_unsigned<S>::value, "Serialize limit for container must be unsigned");
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't write to const map");
        } else {
            if (v.size() > std::numeric_limits<S>::max()) {
                THROW_SERIALIZER_EXCEPTION("Map has too many elements");
            }
        }
        S size = (S)v.size();
        serialize<S>(size);
        for (auto& [key, value] : v) {
            serialize(key);
            serialize(value);
        }
    }

    // uint8_t array
    template <class T> requires is_uint8_array<T>
    void serialize(T& v)
    {
        serialize(v, 0);
    }

    template <class T> requires is_uint8_array<T>
    void serialize(T& v, size_t offset)
    {
        if (offset > v.size()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::serialize array invalid offset");
        }
        size_t size = v.size() - offset;
        if (reader()) {
            read(v.data() + offset, size);
        } else {
            write(v.data() + offset, size);
        }
    }

    // const uint8_t arary
    template <class T> requires is_uint8_array<T>
    void serialize(const T& v)
    {
        serialize(v, 0);
    }

    template <class T> requires is_uint8_array<T>
    void serialize(const T& v, size_t offset)
    {
        if (offset > v.size()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::serialize array invalid offset");
        }
        size_t size = v.size() - offset;
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't write to const array");
        } else {
            write(v.data() + offset, size);
        }
    }

    // array
    template <typename T, size_t S, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    void serialize(std::array<T, S>& v) {
        serialize(v, 0);
    }

    template <typename T, size_t S, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    void serialize(std::array<T, S>& v, size_t offset)
    {
        if (offset > v.size()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::serialize array invalid offset");
        }
        size_t size = v.size() - offset;
        if (reader()) {
            if (m_pos + size * sizeof(T) > m_data.size()) {
                THROW_SERIALIZER_EXCEPTION("Serialzer::serialize array, pos + array_size > size");
            }
            read(v.data() + offset, size * sizeof(T));
        } else {
            write(v.data() + offset, size * sizeof(T));
        }
    }

    // const arary
    template <typename T, size_t S, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    void serialize(const std::array<T, S>& v)
    {
        serialize(v, 0);
    }

    template <typename T, size_t S, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    void serialize(const std::array<T, S>& v, size_t offset)
    {
        if (offset > v.size()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::serialize array invalid offset");
        }
        size_t size = v.size() - offset;
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Can't write to const array");
        } else {
            write(v.data() + offset, size * sizeof(T));
        }
    }

    // const shared_ptr
    template <typename T>
    void serialize(std::shared_ptr<const T>& param)
    {
        if (!param) {
            if (writer()) {
                THROW_SERIALIZER_EXCEPTION("Trying to serialize not initialized shared_ptr");
            }
            if constexpr (has_load<T>) {
                param = T::load(*this);
            } else {
                auto v = std::make_shared<T>();
                serialize(*v);
                param = v;
            }
        } else {
            if (reader()) {
                THROW_SERIALIZER_EXCEPTION("Can't write to const shared ptr");
            }
            const_cast<T*>(param.get())->serialize(*this);
        }
    }

    // shared_ptr
    template <typename T>
    void serialize(std::shared_ptr<T>& param)
    {
        if (!param) {
            if (writer()) {
                THROW_SERIALIZER_EXCEPTION("Trying to serialize not initialized shared_ptr");
            }
            if constexpr (has_load<T>) {
                param = T::load(*this);
            } else {
                param = std::make_shared<T>();
                serialize(*param);
            }
        } else {
            serialize(*param);
        }
    }

    // const const shared_ptr
    template <typename T>
    void serialize(const std::shared_ptr<const T>& param)
    {
        if (!param) {
            THROW_SERIALIZER_EXCEPTION("Trying to serialize not initialized shared_ptr");
        } else {
            if (reader()) {
                THROW_SERIALIZER_EXCEPTION("Can't write to const shared ptr");
            }
            const_cast<T*>(param.get())->serialize(*this);
        }
    }

    // const shared_ptr
    template <typename T>
    void serialize(const std::shared_ptr<T>& param)
    {
        if (!param || reader()) {
            THROW_SERIALIZER_EXCEPTION("Trying to serialize const or null shared_ptr in read action");
        } else {
            serialize(*param);
        }
    }

    // std pair
    template <typename V1, typename V2>
    void serialize(std::pair<V1, V2>& param)
    {
        serialize(param.first);
        serialize(param.second);
    }

    // std pair
    template <typename V1, typename V2>
    void serialize(const std::pair<V1, V2>& param)
    {
        serialize(param.first);
        serialize(param.second);
    }

    // bool
    void serialize(bool& param)
    {
        if (reader()) {
            uint8_t v = get<uint8_t>();
            if (v > 1) {
                THROW_SERIALIZER_EXCEPTION("Invalid value of bool");
            }
            param = (v == 1);
        } else {
            put<uint8_t>(param ? 1 : 0);
        }
    }

    // serializer
    void serialize(Serializer& s)
    {
        BOOST_ASSERT(&s != this);
        uint32_t size = (uint32_t)s.size();
        serialize(size);
        if (size > MAX_SIZE) {
            THROW_SERIALIZER_EXCEPTION("Serializer to serialize is too big");
        }
        if (reader()) {
            s.resize(size);
            s.m_pos = size;
        }
        for (uint32_t i = 0; i < size; ++i) {
            serialize(s.m_data[i]);
        }
    }

    // serialize shared_ptr which can be null
    template<typename T>
    void serializeOptional(T& param)
    {
        static_assert(is_shared_ptr<T>::value == true, "serializeOptional is only for shared_ptr");
        if (reader()) {
            if (param) {
                THROW_SERIALIZER_EXCEPTION("serializeOptional parameter is already initialized");
            }
            uint8_t hasParam = get<uint8_t>();
            if (hasParam > 1) {
                THROW_SERIALIZER_EXCEPTION("Invalid value for serializeOptional");
            }
            if (hasParam == 1) {
                serialize(param);
            } else {
                param = nullptr;
            }
        } else if (param) {
            put<uint8_t>(1);
            serialize(param);
        } else {
            put<uint8_t>(0);
        }
    }

    template<typename T>
    void serializeOptional(const T& param)
    {
        static_assert(is_shared_ptr<T>::value == true, "serializeOptional is only for shared_ptr");
        if (reader()) {
            THROW_SERIALIZER_EXCEPTION("Trying to serialize const or null shared_ptr in read action");
        } else if (param) {
            put<uint8_t>(1);
            serialize(param);
        } else {
            put<uint8_t>(0);
        }
    }

    friend std::ostream& operator<<(std::ostream& out, const Serializer& serializer)
    {
        out << "[" << serializer.base64() << "]";
        return out;
    }

    void writeTo(std::ostream& ofs)
    {
        BOOST_ASSERT(ofs && writer());
        ofs.write(reinterpret_cast<const char*>(buffer()), size());
    }

    std::string base64() const
    {
        return base64url::encode(m_data.data(), size());
    }

    void putCompressedData(const Serializer& s)
    {
        if (!writer()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::putCompressedData, can be used only by writer");
        }
        if (s.size() > MAX_SIZE) {
            THROW_SERIALIZER_EXCEPTION("Serializer to compress is too big");
        }
        std::vector<uint8_t> compressedData(s.size() + 1024);
        unsigned long compressedLength = compressedData.size();
        int ret = compress2(compressedData.data(), &compressedLength, s.buffer(), s.size(), COMPRESSION_LEVEL);
        if (ret != Z_OK || compressedLength == 0 || compressedLength == compressedData.size()) {
            BOOST_THROW_EXCEPTION(SerializerException("Can't compress, zlib returned "s + std::to_string(ret)));
        }
        put<uint32_t>(s.size()); // 4 bytes - size of uncompressed data
        compressedData.resize(compressedLength);
        serialize<uint32_t>(compressedData);
    }

private:
    template <typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
    void serializeBasic(T& param)
    {
        if (reader()) {
            read(param);
        } else {
            write(param);
        }
    }

    // non number
    template <typename T, std::enable_if_t<!std::is_integral_v<T>, bool> = true>
    void serializeBasic(T& param)
    {
        serializeNaN(param);
    }

    // pointer
    template <typename T, std::enable_if_t<std::is_pointer_v<T>, bool> = true>
    void serializeNaN(T& param)
    {
        if (!param) {
            THROW_SERIALIZER_EXCEPTION("Trying to initialize null pointer");
        }
        param->serialize(*this);
    }

    // non pointer
    template <typename T, std::enable_if_t<!std::is_pointer_v<T>, bool> = true>
    void serializeNaN(T& param)
    {
        param.serialize(*this);
    }


    template <class T, std::enable_if_t<std::is_const_v<T>, bool> = true>
    void read(T& param)
    {
        THROW_SERIALIZER_EXCEPTION("Serialzer::read, tried to write to const member");
    }

    template <class T, std::enable_if_t<!std::is_const_v<T>, bool> = true>
    void read(T& param)
    {
        if (!reader()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::read, can be used only by reader");
        }
        if (m_pos + sizeof(T) > m_data.size()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::read, pos + object_size > size");
        }
        memcpy(&param, &m_data[m_pos], sizeof(T));
        m_pos += sizeof(T);
    }

    void read(void* data, size_t size)
    {
        if (size == 0) {
            return;
        }
        if (m_pos + size > m_data.size()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::read, pos + size > m_data.size()");
        }
        memcpy(data, &m_data[m_pos], size);
        m_pos += size;
    }

    template <class T>
    void write(T& param)
    {
        if (m_pos + sizeof(T) > m_data.size()) {
            resize(m_pos + sizeof(T));
        }
        memcpy(&m_data[m_pos], &param, sizeof(T));
        m_pos += sizeof(T);
    }

    void write(const void* data, size_t size)
    {
        if (size == 0) {
            return;
        }
        if (!writer()) {
            THROW_SERIALIZER_EXCEPTION("Serialzer::write, can be used only by writer");
        }
        if (m_pos + size > m_data.size()) {
            resize(m_pos + size);
        }
        memcpy(&m_data[m_pos], data, size);
        m_pos += size;
    }

    void resize(size_t minimum_size)
    {
        if (m_data.capacity() < minimum_size) {
            size_t new_size = std::max(minimum_size, m_data.size() * 2);
            m_data.reserve(new_size);
        }
        m_data.resize(minimum_size);
    }

    bool m_reader;
    std::vector<uint8_t> m_data;
    size_t m_pos = 0;
    std::string m_file;
};

}
