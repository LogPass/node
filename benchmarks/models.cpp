#include "pch.h"

#include <boost/test/unit_test.hpp>
#include <models/user/user.h>

#include "time_tester.h"

using namespace logpass;
BOOST_AUTO_TEST_CASE(models)
{
    std::map<PublicKey, uint8_t> keys;
    for(int i = 0; i < 10; ++i)
        keys.insert({ PublicKey::generateRandom(), 5 });

    std::vector<User_cptr> users;
    users.reserve(1000000);
    {
        TimeTester t("Genereting 1 mln users");
        for (int i = 0; i < 1000000; ++i) {
            users.push_back(User::create(keys.begin()->first, UserId(), 1, 10000 + i, 10));
        }
    }
    users.clear();
    users.reserve(1000000);
    {
        TimeTester t("Cloning 1 mln users with 10 keys");
        auto user = User::create(keys.begin()->first, UserId(), 1, 10, 10);
        user->settings.keys = UserKeys::create(keys);
        for (int i = 0; i < 1000000; ++i) {
            users.push_back(user->clone(1));
        }
    }

}

