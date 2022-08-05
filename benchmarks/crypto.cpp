#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <blockchain/transactions/create_user.h>

#include "time_tester.h"

using namespace logpass;

BOOST_AUTO_TEST_CASE(crypto)
{
    auto keys = PrivateKey::generate(5);
    UserId userId(PublicKey::generateRandom());
    std::map<Hash, MultiSignatures> toValidate;

    {
        TimeTester t("Genereting 10000 multisignatures with 5 signatures each");
        for (int i = 0; i < 10000; ++i) {
            auto hash = Hash::generate(std::to_string(i));
            auto& it = toValidate[hash];
            it.setUserId(userId);
            it.sign("", hash, keys);
        }
    }

    {
        TimeTester t("Validation of 10000 x 5 signatures on single thread");
        for (auto& [hash, signatures] : toValidate) {
            if (!signatures.verify("", hash)) {
                BOOST_TEST_FAIL("Signature verification error");
                return;
            }
        }
    }

    {
        TimeTester t("Validation of 10000 x 5 signatures on 8 threads");
        std::vector<std::thread> threads;
        std::mutex mutex;
        auto it = toValidate.begin();
        for (int i = 0; i < 8; ++i) {
            threads.push_back(std::thread([&] {
                mutex.lock();
                auto& hash = it->first;
                auto& signatures = it->second;
                ++it;
                mutex.unlock();
                if (!signatures.verify("", hash)) {
                    BOOST_TEST_FAIL("Signature verification error");
                    return;
                }
            }));
        }
        for (auto& thread : threads) {
            thread.join();
        }
    }
}

