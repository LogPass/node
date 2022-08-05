#include "pch.h"

#include "blockchain_test.h"

namespace logpass {

BlockchainTest::BlockchainTest(const BlockchainOptions& blockchainOptions, const std::shared_ptr<Database>& database)
    : Blockchain(blockchainOptions, database)
{}

Block_cptr BlockchainTest::createBlock(uint32_t blockId, const std::vector<Transaction_cptr>& transactions,
                                       const PrivateKey& key)
{
    std::promise<Block_cptr> p;
    post([this, blockId, transactions, key, &p] {
        p.set_value(Blockchain::createBlock(blockId, transactions, key));
    });
    return p.get_future().get();
}

Block_cptr BlockchainTest::mineBlock(uint32_t blocksToSkip)
{
    return mineBlock(blocksToSkip, chrono::high_resolution_clock::now() + chrono::seconds(10));
}

Block_cptr BlockchainTest::mineBlock(uint32_t blocksToSkip, const chrono::high_resolution_clock::time_point& deadline)
{
    ASSERT(blocksToSkip < kMinersQueueSize);
    std::promise<Block_cptr> p;
    post([this, blocksToSkip, deadline, &p] {
        p.set_value(Blockchain::mineBlock(getLatestBlockId() + blocksToSkip + 1, deadline));
    });
    return p.get_future().get();
}

bool BlockchainTest::addBlock(const Block_cptr& block, bool mustBeExecuted)
{
    ASSERT(block);
    if (mustBeExecuted)
        m_expectedBlockId = block->getId();
    std::promise<bool> p;
    post([this, &p, block, mustBeExecuted] {
        if (!m_blockTree->addBlock(block, MinerId())) {
            p.set_value(false);
            return;
        }

        Blockchain::update();
        if (mustBeExecuted) {
            p.set_value(m_database->confirmed().blocks.getLatestBlockHeader()->getHash() == block->getHeaderHash());
        } else {
            p.set_value(true);
        }
    });
    return p.get_future().get();
}

bool BlockchainTest::mineAndAddBlock()
{
    return addBlock(mineBlock());
}

void BlockchainTest::setExpectedBlockId(uint32_t blockId)
{
    m_expectedBlockId = blockId;
}

void BlockchainTest::update(bool wait)
{
    if (!wait) {
        post([&] {
            Blockchain::update();
        });
        return;
    }

    std::promise<bool> p;
    post([&] {
        Blockchain::update();
        p.set_value(true);
    });
    p.get_future().get();
}

PostTransactionResult BlockchainTest::postTransaction(Serializer& serializer) noexcept
{
    auto p = std::make_shared<std::promise<std::shared_ptr<PostTransactionResult>>>();
    Blockchain::postTransaction(serializer, (PostTransactionCallback)[this, p](auto result) {
        p->set_value(result);
    });
    auto f = p->get_future();
    if (f.wait_for(chrono::seconds(3)) != std::future_status::ready) {
        return PostTransactionResult(TransactionId(), PostTransactionResult::Status::TIMEOUT);
    }
    return *(f.get());
}

PostTransactionResult BlockchainTest::postTransaction(const Serializer_ptr& serializer) noexcept
{
    ASSERT(serializer);
    return postTransaction(*serializer);
}

PostTransactionResult BlockchainTest::postTransaction(const Transaction_cptr& transaction) noexcept
{
    auto p = std::make_shared<std::promise<std::shared_ptr<PostTransactionResult>>>();
    Blockchain::postTransaction(transaction, (PostTransactionCallback)[this, p](auto result) {
        p->set_value(result);
    });
    auto f = p->get_future();
    if (f.wait_for(chrono::seconds(3)) != std::future_status::ready) {
        return PostTransactionResult(transaction->getId(), PostTransactionResult::Status::TIMEOUT);
    }
    return *(f.get());
}

PostTransactionResult BlockchainTest::postTransactions(const std::vector<Transaction_cptr>& transactions) noexcept
{
    ASSERT(transactions.size() > 0);
    auto promises = std::make_shared<std::vector<std::promise<std::shared_ptr<PostTransactionResult>>>>(transactions.size());
    for (size_t i = 0; i < transactions.size(); ++i) {
        Blockchain::postTransaction(transactions[i], (PostTransactionCallback)[this, i, promises](auto result) {
            promises->at(i).set_value(result);
        });
    }

    chrono::system_clock::time_point deadline = chrono::system_clock::now() +
        chrono::seconds(std::max<size_t>(3, transactions.size() / 100));
    for (auto& promise : *promises) {
        auto future = promise.get_future();
        if (future.wait_until(deadline) != std::future_status::ready) {
            return PostTransactionResult(TransactionId(), PostTransactionResult::Status::TIMEOUT);
        }
        auto result = future.get();
        if (result && !(*result)) {
            return *result;
        }
    }
    return PostTransactionResult(TransactionId(), PostTransactionResult::Status::SUCCESS);
}

Block_cptr BlockchainTest::getBlock(uint32_t blockId) const
{
    return m_database->confirmed().blocks.getBlock(blockId);
}

Block_cptr BlockchainTest::getBlockAfter(uint32_t blockId) const
{
    auto header = m_database->confirmed().blocks.getNextBlockHeader(blockId);
    if (!header)
        return nullptr;
    return getBlock(header->getId());
}

uint32_t BlockchainTest::getExpectedBlockId() const
{
    return std::max(m_expectedBlockId, getLatestBlockId() + 1);
}

}
