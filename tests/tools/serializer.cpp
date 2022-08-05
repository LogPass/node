#include "pch.h"

#include <boost/test/unit_test.hpp>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(serializer);

BOOST_AUTO_TEST_CASE(map)
{
    Serializer s;
    std::map<int, int> map;
    for (int i = 0; i < 10; ++i) {
        map[i] = i;
    }
    s(map);
    s.switchToReader();
    std::map<int, int> map2;
    s(map2);
    BOOST_TEST_REQUIRE(map == map2);
}

BOOST_AUTO_TEST_CASE(map_uint8)
{
    Serializer s;
    std::map<int, int> map;
    for (int i = 0; i < 255; ++i) {
        map[i] = i;
    }
    s.serialize<uint8_t>(map);
    s.switchToReader();
    std::map<int, int> map2;
    s.serialize<uint8_t>(map2);
    BOOST_TEST_REQUIRE(map == map2);
}

BOOST_AUTO_TEST_CASE(map_uint8_overflow)
{
    Serializer s;
    std::map<int, int> map;
    for (int i = 0; i < 256; ++i) {
        map[i] = i;
    }
    BOOST_REQUIRE_THROW(s.serialize<uint8_t>(map), SerializerException);
}

BOOST_AUTO_TEST_CASE(map_uint32)
{
    Serializer s;
    std::map<uint16_t, uint16_t> map;
    for (int i = 0; i < 1000; ++i) {
        map[i] = i;
    }
    s.serialize<uint32_t>(map);
    s.switchToReader();
    std::map<uint16_t, uint16_t> map2;
    s.serialize<uint32_t>(map2);
    BOOST_TEST_REQUIRE(map == map2);
    BOOST_TEST_REQUIRE(s.size() == 1000 * 4 + 4);
}

BOOST_AUTO_TEST_CASE(map_const_uint32)
{
    Serializer s;
    std::map<uint16_t, uint16_t> map;
    for (int i = 0; i < 1000; ++i) {
        map[i] = i;
    }
    const std::map<uint16_t, uint16_t> const_map(map);
    map.clear();
    s.serialize<uint32_t>(const_map);
    s.switchToReader();
    std::map<uint16_t, uint16_t> map2;
    s.serialize<uint32_t>(map2);
    BOOST_TEST_REQUIRE(const_map == map2);
    BOOST_TEST_REQUIRE(s.size() == 1000 * 4 + 4);
}

BOOST_AUTO_TEST_CASE(map_const_uint32_invalid)
{
    Serializer s;
    std::map<uint16_t, uint16_t> map;
    for (int i = 0; i < 1000; ++i) {
        map[i] = i;
    }
    const std::map<uint16_t, uint16_t> const_map(map), const_map2;
    map.clear();
    s.serialize<uint32_t>(const_map);
    s.switchToReader();
    BOOST_REQUIRE_THROW(s.serialize<uint32_t>(const_map2), SerializerException);
}

BOOST_AUTO_TEST_CASE(map_raw)
{
    Serializer s(std::vector<uint8_t>({ 0x02, 0x00, 0x01, 0x05, 0x02, 0x06 }));
    std::map<uint8_t, uint8_t> map;
    BOOST_REQUIRE_NO_THROW(s.serialize(map));
    BOOST_TEST_REQUIRE(map[1] == 0x05);
    BOOST_TEST_REQUIRE(map[2] == 0x06);
}

BOOST_AUTO_TEST_CASE(map_unsorted)
{
    Serializer s(std::vector<uint8_t>({ 0x02, 0x00, 0x01, 0x00, 0x00, 0x00 }));
    std::map<uint8_t, uint8_t> map;
    BOOST_REQUIRE_THROW(s.serialize(map), SerializerException);
}

BOOST_AUTO_TEST_CASE(map_duplicated)
{
    Serializer s(std::vector<uint8_t>({ 0x02, 0x00, 0x01, 0x00, 0x01, 0x00 }));
    std::map<uint8_t, uint8_t> map;
    BOOST_REQUIRE_THROW(s.serialize(map), SerializerException);
}

BOOST_AUTO_TEST_CASE(map_invalid)
{
    Serializer s(std::vector<uint8_t>({ 0x02, 0x00, 0x01, 0x00, 0x03 }));
    std::map<uint8_t, uint8_t> map;
    BOOST_REQUIRE_THROW(s.serialize(map), SerializerException);
}

BOOST_AUTO_TEST_CASE(map_not_empty)
{
    Serializer s(std::vector<uint8_t>({ 0x02, 0x00, 0x01, 0x05, 0x02, 0x06 }));
    std::map<uint8_t, uint8_t> map = { { 0x02, 0x03 } };
    BOOST_REQUIRE_THROW(s.serialize(map), SerializerException);
}

BOOST_AUTO_TEST_CASE(stringstream)
{
    std::vector<uint32_t> v = { 1,2,3,4,5 };
    Serializer s;
    s(v);
    std::stringstream stream;
    s.writeTo(stream);
    stream.seekg(0, stream.beg);
    Serializer s2(stream);
    std::vector<uint32_t> v2;
    s2(v2);
    BOOST_TEST_REQUIRE(v == v2);
    BOOST_TEST_REQUIRE(s2.eof());
}

BOOST_AUTO_TEST_CASE(get_put)
{
    Serializer s;
    s.put<uint8_t>(0xFE);
    s.put<uint16_t>(0xFDFD);
    s.put<uint32_t>(0xFCFCFCFCu);
    s.put<uint64_t>(0xFBFBFBFBFBFBFBFBull);
    s.put<std::string>("Test");
    s.put<std::vector<uint8_t>>({ 1,2,3 });
    s.switchToReader();
    BOOST_TEST_REQUIRE(s.get<uint8_t>() == 0xFE);
    BOOST_TEST_REQUIRE(s.get<uint16_t>() == 0xFDFD);
    BOOST_TEST_REQUIRE(s.get<uint32_t>() == 0xFCFCFCFCu);
    BOOST_TEST_REQUIRE(s.get<uint64_t>() == 0xFBFBFBFBFBFBFBFBull);
    BOOST_TEST_REQUIRE(s.get<std::string>() == "Test"s);
    BOOST_TEST_REQUIRE(s.get<std::vector<uint8_t>>() == std::vector<uint8_t>({ 1, 2, 3 }));
}

BOOST_AUTO_TEST_CASE(string)
{
    std::string value;
    for (size_t i = 0; i < 1024 * 1024; ++i) {
        value += (char)(i % 256);
    }
    Serializer s;
    s.serialize<uint32_t>(value);
    s.switchToReader();
    std::string value2;
    s.serialize<uint32_t>(value2);
    BOOST_TEST_REQUIRE(value == value2);
    BOOST_TEST_REQUIRE(s.eof());
}

BOOST_AUTO_TEST_CASE(boolean)
{
    bool b1[] = { true, false, true };
    Serializer s;
    s(b1[0]);
    s(b1[1]);
    s(b1[2]);
    s.switchToReader();
    bool b2[] = { false, false, false};
    s(b2[0]);
    s(b2[1]);
    s(b2[2]);
    BOOST_TEST_REQUIRE(b1[0] == b2[0]);
    BOOST_TEST_REQUIRE(b1[1] == b2[1]);
    BOOST_TEST_REQUIRE(b1[2] == b2[2]);

    // test invalid values
    for (uint8_t v = 2; v < 255; ++v) {
        Serializer s(std::vector<uint8_t>({ v }));
        bool b = false;
        BOOST_REQUIRE_THROW(s(b), SerializerException);
    }
}

BOOST_AUTO_TEST_CASE(base64)
{
    Serializer s;
    std::string str = "123456789";
    s(str);
    BOOST_TEST_REQUIRE(s.base64() == "CQAxMjM0NTY3ODk=");
    auto s2 = Serializer::fromBase64(s.base64());
    std::string str2;
    s2(str2);
    BOOST_TEST_REQUIRE(str == str2);
    BOOST_TEST_REQUIRE(s2.eof());
    auto s3 = Serializer::fromBase64("CQAxMjM0NTY3ODk");
    std::string str3;
    s3(str3);
    BOOST_TEST_REQUIRE(str == str3);
    BOOST_TEST_REQUIRE(s3.eof());
}

BOOST_AUTO_TEST_CASE(compression)
{
    Serializer s;
    std::string str = "123456789";
    s(str);
    Serializer s2;
    s2.putCompressedData(s);
    s2.switchToReader();
    Serializer s3 = Serializer::fromCompressedData(s2);
    std::string str2;
    s3(str2);
    BOOST_TEST_REQUIRE(str == str2);
    BOOST_TEST_REQUIRE(s3.eof());
}

BOOST_AUTO_TEST_CASE(serialize_optional)
{
    auto value = std::make_shared<std::string>("0123456789");
    Serializer s;
    s.serializeOptional(value);
    s.switchToReader();
    std::shared_ptr<std::string> value2, value3;
    s.serializeOptional(value2);
    BOOST_TEST_REQUIRE(value2);
    BOOST_TEST_REQUIRE(*value == *value2);
    BOOST_TEST_REQUIRE(s.eof());
    value2 = nullptr;
    Serializer s2;
    s2.serializeOptional(value2);
    s2.switchToReader();
    s2.serializeOptional(value3);
    BOOST_TEST_REQUIRE(value2 == nullptr);
    BOOST_TEST_REQUIRE(value3 == nullptr);
    BOOST_TEST_REQUIRE(s2.eof());
}

BOOST_AUTO_TEST_CASE(uint8_array)
{
    std::array<uint8_t, 5> arr1 = { 1, 2, 3, 4, 0xFF };
    Serializer s;
    s(arr1);
    BOOST_TEST_REQUIRE(s.pos() == 5);
    BOOST_TEST_REQUIRE(s.size() == 5);
    s.switchToReader();
    std::array<uint8_t, 5> arr2;
    s(arr2);
    BOOST_TEST_REQUIRE(arr1 == arr2);
}

BOOST_AUTO_TEST_CASE(uint8_array_with_offset)
{
    std::array<uint8_t, 5> arr1 = { 1, 2, 3, 4, 0xFF };
    Serializer s;
    s.serialize(arr1, 2);
    BOOST_TEST_REQUIRE(s.pos() == 3);
    BOOST_TEST_REQUIRE(s.size() == 3);
    s.switchToReader();
    std::array<uint8_t, 5> arr2 = {};
    s.serialize(arr2, 2);
    BOOST_TEST_REQUIRE(arr2[0] == 0);
    BOOST_TEST_REQUIRE(arr2[1] == 0);
    BOOST_TEST_REQUIRE(arr1[2] == arr2[2]);
    BOOST_TEST_REQUIRE(arr1[3] == arr2[3]);
    BOOST_TEST_REQUIRE(arr1[4] == arr2[4]);
}


BOOST_AUTO_TEST_CASE(uint32_array)
{
    std::array<uint32_t, 5> arr1 = { 1, 2, 3, 4, 0xFFFFEE };
    Serializer s;
    s(arr1);
    BOOST_TEST_REQUIRE(s.pos() == 5 * sizeof(uint32_t));
    BOOST_TEST_REQUIRE(s.size() == 5 * sizeof(uint32_t));
    s.switchToReader();
    std::array<uint32_t, 5> arr2;
    s(arr2);
    BOOST_TEST_REQUIRE(arr1 == arr2);
    BOOST_TEST_REQUIRE(arr2[4] == 0xFFFFEE);
}

BOOST_AUTO_TEST_CASE(uint64_array_with_offset)
{
    std::array<uint64_t, 5> arr1 = { 1, 2, 3, 4, 0xFFFFFFFFAA };
    Serializer s;
    s.serialize(arr1, 2);
    BOOST_TEST_REQUIRE(s.pos() == 3 * sizeof(uint64_t));
    BOOST_TEST_REQUIRE(s.size() == 3 * sizeof(uint64_t));
    s.switchToReader();
    std::array<uint64_t, 5> arr2 = {};
    s.serialize(arr2, 2);
    BOOST_TEST_REQUIRE(arr2[0] == 0);
    BOOST_TEST_REQUIRE(arr2[1] == 0);
    BOOST_TEST_REQUIRE(arr1[2] == arr2[2]);
    BOOST_TEST_REQUIRE(arr1[3] == arr2[3]);
    BOOST_TEST_REQUIRE(arr1[4] == arr2[4]);
}

BOOST_AUTO_TEST_SUITE_END();
