#pragma once

#include <blockchain/blockchain_test.h>
#include <database/database_fixture.h>
#include "test_user.h"

namespace logpass {

struct BlockchainFixture : public DatabaseFixture<> {
    BlockchainFixture(BlockchainOptions blockchainOptions = BlockchainOptions())
    {
        reinitialize(blockchainOptions);
    }

    ~BlockchainFixture()
    {
        blockchain.reset();
    }

    void reinitialize(BlockchainOptions blockchainOptions = BlockchainOptions())
    {
        blockchain.reset();
        DatabaseFixture::reinitialize();
        if (blockchainOptions.firstBlocks.empty()) {
            blockchainOptions.initialize = true;
        }
        if (!blockchainOptions.minerKey.isValid()) {
            blockchainOptions.minerKey = PrivateKey::generate();
        }        
        blockchain = SharedThread<BlockchainTest>(blockchainOptions, db);
        user = TestUser(blockchain->getUserId(), std::vector<PrivateKey>{ blockchain->getMinerKey() }, blockchain, db);
    }

    TestUser createUser()
    {
        PrivateKey key = PrivateKey::generate();
        auto result = user.createUser(key.publicKey());
        ASSERT(result);
        return TestUser(UserId(key.publicKey()), { key }, blockchain, db);
    }

    SharedThread<BlockchainTest> blockchain;
    TestUser user;
};

}
