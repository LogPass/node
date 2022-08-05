#include "pch.h"

#include <boost/test/unit_test.hpp>

using namespace logpass;

BOOST_AUTO_TEST_SUITE(transactionId);

BOOST_AUTO_TEST_CASE(basic)
{
    auto hash = Hash::generate("Test");
    auto transactionId = TransactionId(17, 34144246, 10936, hash);
    BOOST_TEST_REQUIRE(transactionId.isValid());
    BOOST_TEST_REQUIRE(transactionId.getType() == 17);
    BOOST_TEST_REQUIRE(transactionId.getBlockId() == 34144246);
    BOOST_TEST_REQUIRE(transactionId.getSize() == 10936);
    BOOST_TEST_REQUIRE(transactionId.getHash() == hash);
    BOOST_TEST_REQUIRE(transactionId.toString() == "Agj_9hEquFMuqr2VdIgNv3a5uMwAgywgpuwRPWgimVUNem4PNF4l");

    BOOST_TEST_REQUIRE(transactionId == TransactionId("Agj_9hEquFMuqr2VdIgNv3a5uMwAgywgpuwRPWgimVUNem4PNF4l"s));
    BOOST_TEST_REQUIRE(transactionId == TransactionId("Agj/9hEquFMuqr2VdIgNv3a5uMwAgywgpuwRPWgimVUNem4PNF4l"s));
    BOOST_TEST_REQUIRE(transactionId != TransactionId("Ogj_9hEquFMuqr2VdIgNv3a5uMwAgywgpuwRPWgimVUNem4PNF4l"s));

    Serializer s;
    s(transactionId);
    s.switchToReader();
    TransactionId transactionId2;
    s(transactionId2);
    BOOST_TEST_REQUIRE(transactionId2.isValid());
    BOOST_TEST_REQUIRE(transactionId2.getType() == 17);
    BOOST_TEST_REQUIRE(transactionId2.getBlockId() == 34144246);
    BOOST_TEST_REQUIRE(transactionId2.getSize() == 10936);
    BOOST_TEST_REQUIRE(transactionId2.getHash() == hash);
    BOOST_TEST_REQUIRE(transactionId2.toString() == "Agj_9hEquFMuqr2VdIgNv3a5uMwAgywgpuwRPWgimVUNem4PNF4l");
    BOOST_TEST_REQUIRE(transactionId == transactionId2);

    BOOST_TEST_REQUIRE(TransactionId(1, 1, 1, hash) < TransactionId(1, 2, 1, hash));
    BOOST_TEST_REQUIRE(TransactionId(1, 1, 1, hash) < TransactionId(2, 1, 1, hash));
    BOOST_TEST_REQUIRE(TransactionId(1, 1, 1, hash) < TransactionId(1, 1, 2, hash));
    BOOST_TEST_REQUIRE(TransactionId(1, 10, 1, hash) < TransactionId(1, 256, 1, hash));
    BOOST_TEST_REQUIRE(TransactionId(1, 255, 1, hash) < TransactionId(1, 65531, 1, hash));

    // test ordering and transaction types
    for (uint32_t i = 2; i <= 0xFF; ++i) {
        TransactionId id1(1, i, rand() % kTransactionMaxSize, Hash::generateRandom());
        TransactionId id2(1, i + 1, rand() % kTransactionMaxSize, Hash::generateRandom());
        TransactionId id3(i, i, rand() % kTransactionMaxSize, Hash::generateRandom());
        TransactionId id4(i, i + 1, rand() % kTransactionMaxSize, Hash::generateRandom());
        BOOST_TEST_REQUIRE(id1 < id2);
        BOOST_TEST_REQUIRE(id1 < id3);
        BOOST_TEST_REQUIRE(id2 < id4);
        BOOST_TEST_REQUIRE(id3 < id4);
        BOOST_TEST_REQUIRE(id3.getType() == i);
        BOOST_TEST_REQUIRE(id4.getType() == i);
    }

}

BOOST_AUTO_TEST_SUITE_END();
