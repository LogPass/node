#include "pch.h"

#include <boost/test/unit_test.hpp>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(crypto);
BOOST_AUTO_TEST_SUITE(multi_signatures);

BOOST_AUTO_TEST_CASE(general)
{
    auto keys = PrivateKey::generate(5);
    Hash hash = Hash::generate("X");
    MultiSignatures signatures;
    signatures.setPublicKey(keys[0].publicKey());
    signatures.setUserId(UserId(keys[0].publicKey()));
    signatures.sign("TEST", hash, { keys[0] });
    BOOST_TEST_REQUIRE(signatures.verify("TEST", hash));
    BOOST_TEST_REQUIRE(!signatures.verify("TES", hash));
    BOOST_TEST_REQUIRE(!signatures.verify("TEST", Hash::generate("Y")));
    signatures.setPublicKey(keys[1].publicKey());
    BOOST_TEST_REQUIRE(!signatures.verify("TEST", hash));
}

BOOST_AUTO_TEST_CASE(multi_signatures)
{
    auto keys = PrivateKey::generate(5);
    Hash hash = Hash::generate("X");
    MultiSignatures signatures;
    signatures.setPublicKey(keys[0].publicKey());
    signatures.setUserId(UserId(keys[0].publicKey()));
    signatures.sign("TEST", hash, { keys[0], keys[1] });
    BOOST_TEST_REQUIRE(signatures.verify("TEST", hash));
    BOOST_TEST_REQUIRE(!signatures.verify("TES", hash));
    BOOST_TEST_REQUIRE(!signatures.verify("TEST", Hash::generate("Y")));
    signatures.setPublicKey(keys[1].publicKey());
    BOOST_TEST_REQUIRE(!signatures.verify("TEST", hash));
}

BOOST_AUTO_TEST_CASE(user_signatures)
{
    auto keys = PrivateKey::generate(5);
    Hash hash = Hash::generate("X");
    MultiSignatures signatures;
    signatures.setPublicKey(keys[0].publicKey());
    signatures.setUserId(UserId(keys[0].publicKey()));
    signatures.sign("TEST", hash, { keys[0], keys[1] });
    BOOST_TEST_REQUIRE(signatures.verify("TEST", hash));
    BOOST_TEST_REQUIRE(!signatures.verify("TES", hash));
    BOOST_TEST_REQUIRE(!signatures.verify("TEST", Hash::generate("Y")));
}

BOOST_AUTO_TEST_CASE(duplicated_user)
{
    auto keys = PrivateKey::generate(5);
    Hash hash = Hash::generate("X");
    MultiSignatures signatures;
    signatures.setPublicKey(keys[0].publicKey());
    signatures.setUserId(UserId(keys[0].publicKey()));
    signatures.setSponsorId(UserId(keys[0].publicKey()));
    signatures.sign("TEST", hash, { keys[0] });
    BOOST_TEST_REQUIRE(!signatures.validate());
    BOOST_TEST_REQUIRE(!signatures.verify("TEST", hash));
}

BOOST_AUTO_TEST_CASE(too_many_keys)
{
    auto key = PrivateKey::generate();
    auto keys = PrivateKey::generate(11);
    Hash hash = Hash::generate("X");
    MultiSignatures signatures;
    signatures.setPublicKey(key.publicKey());
    signatures.setUserId(UserId(key.publicKey()));
    signatures.sign("TEST", hash, keys );
    BOOST_TEST_REQUIRE(!signatures.validate());
    signatures.sign("TEST", hash, { keys[0] });
    BOOST_TEST_REQUIRE(!signatures.verify("TEST", hash));
}

BOOST_AUTO_TEST_CASE(serialization)
{
    auto key = PrivateKey::generate();
    auto keys = PrivateKey::generate(5);
    auto supervisors = PrivateKey::generate(5);
    Hash hash = Hash::generate("X");
    MultiSignatures signatures;
    signatures.setPublicKey(key.publicKey());
    signatures.setUserId(UserId(key.publicKey()));
    signatures.sign("TEST", hash, keys);
    BOOST_TEST_REQUIRE(signatures.validate());
    Serializer s;
    s(signatures);
    s.switchToReader();
    MultiSignatures signatures2;
    s(signatures2);
    BOOST_TEST_REQUIRE(signatures.validate());
    Serializer s2;
    s2(signatures2);
    BOOST_TEST_REQUIRE(s.base64() == s2.base64());
}

BOOST_AUTO_TEST_SUITE_END();
BOOST_AUTO_TEST_SUITE_END();
