#include "pch.h"

#include <boost/test/unit_test.hpp>

#include <filesystem/filesystem.h>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(filesystem);

BOOST_AUTO_TEST_CASE(write_and_read)
{
    auto fs = Filesystem::createTemporaryFilesystem();

    Hash hash1 = Hash::generateRandom();
    Hash hash2 = Hash::generateRandom();

    auto s = std::make_shared<Serializer>();
    (*s)(hash1);
    (*s)(hash2);

    fs->write("test.bin", s);
    auto s2 = fs->read("test.bin");
    BOOST_TEST_REQUIRE(s2);
    BOOST_TEST_REQUIRE(s->size() == s2->size());
    BOOST_TEST_REQUIRE(s->base64() == s2->base64());
    Hash hash3, hash4;
    (*s2)(hash3);
    (*s2)(hash4);
    BOOST_TEST_REQUIRE(hash1 == hash3);
    BOOST_TEST_REQUIRE(hash2 == hash4);
}

BOOST_AUTO_TEST_CASE(write_and_read_array)
{
    auto fs = Filesystem::createTemporaryFilesystem();

    std::array<uint16_t, 4096> arr1, arr2;
    for (size_t i = 0; i < arr1.size(); ++i) {
        arr1[i] = i;
    }

    auto s = std::make_shared<Serializer>();
    (*s)(arr1);

    fs->write("test.bin", s);
    auto s2 = fs->read("test.bin");
    BOOST_TEST_REQUIRE(s2);
    BOOST_TEST_REQUIRE(s->size() == s2->size());
    BOOST_TEST_REQUIRE(s->base64() == s2->base64());
    (*s2)(arr2);
    BOOST_TEST_REQUIRE(arr1 == arr2);
}

BOOST_AUTO_TEST_CASE(write_and_read_json)
{
    auto fs = Filesystem::createTemporaryFilesystem();

    std::vector<uint16_t> arr1(4096, 0);
    for (size_t i = 0; i < arr1.size(); ++i) {
        arr1[i] = i;
    }

    json j1 = arr1;
    auto s = std::make_shared<Serializer>();
    s->readFrom(j1.dump());
    fs->write("test.json", s);
    auto j2 = fs->readJSON("test.json");
    BOOST_TEST_REQUIRE(j2);
    auto arr2 = j2->get<std::vector<uint16_t>>();
    BOOST_TEST_REQUIRE(arr1 == arr2);
}

BOOST_AUTO_TEST_CASE(invalid_file)
{
    auto fs = Filesystem::createTemporaryFilesystem();
    auto s = fs->read("invalid_file.bin");
    BOOST_TEST_REQUIRE(!s);
    auto j = fs->readJSON("invalid_file.json");
    BOOST_TEST_REQUIRE(!j);
}

BOOST_AUTO_TEST_SUITE_END();
