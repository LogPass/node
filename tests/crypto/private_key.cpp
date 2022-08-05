#include "pch.h"

#include <boost/test/unit_test.hpp>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(private_key);

BOOST_AUTO_TEST_CASE(load_from_pem)
{
    // valid key without password
    const std::string key1pem = "-----BEGIN PRIVATE KEY-----\n"
        "MC4CAQAwBQYDK2VwBCIEIIPGZDMGO1b1szNspxZ8WvEHwBP03Zz5x8ljTrFWXzAD\n"
        "-----END PRIVATE KEY-----";
    PrivateKey key1 = PrivateKey::fromPEM(key1pem, "");
    BOOST_TEST_REQUIRE(key1.isValid());
    BOOST_TEST_REQUIRE(key1.publicKey().toBase64() == "AYH8KWV-QIC2_S6pmcAU9Y2xSgEPydswZWkmHxYK2C_v");
    // valid key with valid password
    const std::string key2pem = "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
        "MIGKME4GCSqGSIb3DQEFDTBBMCkGCSqGSIb3DQEFDDAcBAjyBlc6iaf9iwICCAAw\n"
        "DAYIKoZIhvcNAgkFADAUBggqhkiG9w0DBwQIUJpMv8UQsCsEOKAiufUexe0WEuPJ\n"
        "mV/xprc18r/qd/1Mkkq/RtnE1zjeckKZujqYiPVjVyu5jrElKUiTohGQ62Xl\n"
        "-----END ENCRYPTED PRIVATE KEY-----";
    PrivateKey key2 = PrivateKey::fromPEM(key2pem, "test");
    BOOST_TEST_REQUIRE(key2.isValid());
    BOOST_TEST_REQUIRE(key2.publicKey().toBase64() == "AXOQtDVl7CiDQ0m40kIi2LfhHAPY2-LczgyLPKV-Ey18");
    // valid key with invalid password
    BOOST_REQUIRE_THROW(PrivateKey::fromPEM(key2pem, ""), CryptoException);
    BOOST_REQUIRE_THROW(PrivateKey::fromPEM(key2pem, "test1"), CryptoException);

    // valid key but wrong type
    const std::string key3pem = "-----BEGIN PRIVATE KEY-----"
        "MIIBVAIBADANBgkqhkiG9w0BAQEFAASCAT4wggE6AgEAAkEAzuIJxRMVptfDiLuu"
        "TidpAVMO4RZmTBRhw8S2FaCzSffACrL5giiN3UZou/EJ7Rfgh3rqonWbSltDN1Gu"
        "wLgTiwIDAQABAkAEhTvhaoFWm/BR1IgCfSn2NXJlyK+Dig540JuJ7XD9dLpWLt1t"
        "Vxofj7AcGBireQfbbvm4UfY3cCHRKcJ1Nb0BAiEA64XUVy/oaLE1IMVvrHWgF0Kt"
        "V2dbGSiXC5Gt1idaLfsCIQDg3sFipGP+y+wn0rbcHRA4BhWXpbU6X48TqUqHQ4WL"
        "sQIgWiyPNCJ/lTXj5XwyWZFfjghVXdWQp31G94L7T7cZa+kCICV+/2AjaUkjV82M"
        "JVGuxvWQjikrSHzjUkhFAlhruekBAiEAhRE3aGSF/RoJB8Lo1oDjCD4X0LPkw1j5"
        "TCnaLYERUrg="
        "-----END PRIVATE KEY-----";
    BOOST_REQUIRE_THROW(PrivateKey::fromPEM(key3pem, ""), CryptoException);
    // invalid key
    const std::string key4pem = "-----BEGIN PRIVATE KEY-----\n"
        "MC4CAQAwBQYDK2VwBCIEIIPGZDMGO1b1szNspxZ8WvEHwBP03Zz5x8ljTrFWXzA\n"
        "-----END PRIVATE KEY-----";
    BOOST_REQUIRE_THROW(PrivateKey::fromPEM(key4pem, ""), CryptoException);
}

BOOST_AUTO_TEST_SUITE_END();
