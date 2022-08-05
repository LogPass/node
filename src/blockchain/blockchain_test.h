#pragma once

#include "blockchain.h"

namespace logpass {

// Class for testing blockchain
class BlockchainTest : public Blockchain {
protected:
    friend class SharedThread<BlockchainTest>;
    BlockchainTest(const BlockchainOptions& blockchainOptions, const std::shared_ptr<Database>& database);

public:
    void check() override {}

    // creates new block
    Block_cptr createBlock(uint32_t blockId, const std::vector<Transaction_cptr>& transactions, const PrivateKey& key);

    // mines new block and returns it
    Block_cptr mineBlock(uint32_t blocksToSkip = 0);
    Block_cptr mineBlock(uint32_t blocksToSkip, const chrono::high_resolution_clock::time_point& deadline);

    // adds new block
    bool addBlock(const Block_cptr& block, bool mustBeExecuted = true);

    // mines and adds block
    bool mineAndAddBlock();

    // sets expected block id
    void setExpectedBlockId(uint32_t blockId);

    // updates block tree
    void update(bool wait = true);

    // allow to post transactions with callback
    using Blockchain::postTransaction;

    // post new serialized transaction, returns true on success
    PostTransactionResult postTransaction(Serializer& serializer) noexcept;
    // post new serialized transaction, returns true on success
    PostTransactionResult postTransaction(const Serializer_ptr& serializer) noexcept;
    // post new transaction, returns true on success
    PostTransactionResult postTransaction(const Transaction_cptr& transaction) noexcept;
    // post multiple new transaction, returns true on success
    PostTransactionResult postTransactions(const std::vector<Transaction_cptr>& transactions) noexcept;

    // returns block from database
    Block_cptr getBlock(uint32_t blockId) const;

    // returns fisrt block after given block id from database
    Block_cptr getBlockAfter(uint32_t blockId) const;

    // returns exptected id of new block
    uint32_t getExpectedBlockId() const override;

    UserId getUserId() const
    {
        return UserId(getMinerKey().publicKey());
    }

    bool canMineBlocks() const override
    {
        return false;
    }

protected:
    uint32_t m_expectedBlockId = 0;
};

}
