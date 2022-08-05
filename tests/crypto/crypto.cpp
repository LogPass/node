#include "pch.h"

#include <boost/test/unit_test.hpp>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(crypto);

BOOST_AUTO_TEST_CASE(hash)
{
    auto hash = Hash::generate("Test");
    BOOST_TEST_REQUIRE(hash.isValid());
    BOOST_TEST_REQUIRE(hash.toString() == "Uy6qvZV0iA2_drm4zACDLCCm7BE9aCKZVQ16bg80XiU=");
    BOOST_TEST_REQUIRE(hash == Hash("Uy6qvZV0iA2_drm4zACDLCCm7BE9aCKZVQ16bg80XiU="s));
    BOOST_TEST_REQUIRE(hash == Hash("Uy6qvZV0iA2_drm4zACDLCCm7BE9aCKZVQ16bg80XiU"s));
    BOOST_TEST_REQUIRE(hash == Hash("Uy6qvZV0iA2/drm4zACDLCCm7BE9aCKZVQ16bg80XiU="s));
    BOOST_TEST_REQUIRE(hash != Hash("Ay6qvZV0iA2_drm4zACDLCCm7BE9aCKZVQ16bg80XiU"s));
    BOOST_TEST_REQUIRE(!Hash("Uy6qvZV0iA2_drm4zACDLCCm7BE9aCKZVQ16bg80XiU=="s).isValid());
    BOOST_TEST_REQUIRE(!Hash("Ay6qvZV0iA2/drm4zACDLCCm7BE9aCKZVQ16bg80XiU"s).isValid());
    BOOST_TEST_REQUIRE(!Hash("Ay6qvZV0iA2/drm4zACDLCCm7BE9aCKZVQ16bg80XiU=="s).isValid());
    BOOST_TEST_REQUIRE(!Hash("@y6qvZV0iA2/drm4zACDLCCm7BE9aCKZVQ16bg80XiU="s).isValid());
    BOOST_TEST_REQUIRE(Hash::generateRandom() != Hash::generateRandom());
}

BOOST_AUTO_TEST_CASE(key_generator)
{
    auto key1 = PrivateKey::generate();
    auto key2 = PrivateKey::generate();
    BOOST_TEST_REQUIRE(key1.publicKey() != key2.publicKey());
    BOOST_TEST_REQUIRE(key1.publicKey() == key1.publicKey());
    BOOST_TEST_REQUIRE(key2.publicKey().toString() == key2.publicKey().toString());
}

BOOST_AUTO_TEST_CASE(signing)
{
    auto key1 = PrivateKey::generate();
    auto key2 = PrivateKey::generate();
    auto hash1 = Hash::generate("Test");
    auto hash2 = Hash::generate("Test2");
    Serializer s1, s2;
    s1(hash1);
    s2(hash2);
    auto signature1 = key1.sign("TEST", s1);
    auto signature2 = key2.sign("TEST", s1);
    auto signature3 = key1.sign("TEST", s2);
    auto signature4 = key1.sign("TEST2", s1);
    BOOST_TEST_REQUIRE(key1.verify("TEST", s1, signature1));
    BOOST_TEST_REQUIRE(!key2.verify("TEST", s1, signature1));
    BOOST_TEST_REQUIRE(!key1.verify("TEST2", s1, signature1));
    BOOST_TEST_REQUIRE(!key1.verify("TEST", s2, signature1));
    BOOST_TEST_REQUIRE(!key1.verify("TEST", s1, signature2));
    BOOST_TEST_REQUIRE(key2.verify("TEST", s1, signature2));
    BOOST_TEST_REQUIRE(!key2.verify("TEST2", s2, signature2));
    BOOST_TEST_REQUIRE(!key2.verify("TEST", s1, signature3));
    BOOST_TEST_REQUIRE(!key2.verify("TEST", s2, signature1));
    BOOST_TEST_REQUIRE(key1.verify("TEST", s2, signature3));
    BOOST_TEST_REQUIRE(key1.verify("TEST2", s1, signature4));
    BOOST_TEST_REQUIRE(!key1.verify("TEST", s1, signature4));
    BOOST_TEST_REQUIRE(!key1.verify("TEST2", s2, signature4));
}

BOOST_AUTO_TEST_CASE(signing_custom_prefix)
{
    auto hash = Hash::generate("Test");
    Serializer s;
    s(hash);
    auto key = PrivateKey::generate();
    auto signature = key.sign("TEST", s);
    BOOST_TEST_REQUIRE(key.publicKey().verify("TEST", s, signature));
    BOOST_TEST_REQUIRE(!key.publicKey().verify("TEST2", s, signature));
}

BOOST_AUTO_TEST_SUITE_END();
