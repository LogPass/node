/** @file */

#pragma once

#include "block/block.h"
#include "crypto_verifier.h"
#include "events.h"
#include "transactions/transaction.h"
#include "database/database.h"

#include "bans.h"
#include "block_tree.h"
#include "blockchain_options.h"
#include "pending_transactions.h"
#include "post_transaction_result.h"

namespace logpass {

using PostTransactionCallback = SafeCallback<PostTransactionResult>;

class Blockchain : public EventLoopThread {
protected:
    friend class SharedThread<Blockchain>;

    Blockchain(const BlockchainOptions& options, const std::shared_ptr<Database>& database);
    void start() override;
    void stop() override;

public:
    virtual ~Blockchain();

    // callbacks are valid as long as returned unique_ptr exists
    std::unique_ptr<EventsListener> registerEventsListener(EventsListenerCallbacks&& callbacks) noexcept;

    void postTransaction(Serializer& serializer, PostTransactionCallback&& cb) noexcept;
    void postTransaction(const Serializer_ptr& serializer, PostTransactionCallback&& cb) noexcept;
    void postTransaction(const Transaction_cptr& transaction, PostTransactionCallback&& cb) noexcept;

    // adds transaction to pending transactions without executing them, they will be executed later
    size_t addTransactions(const std::vector<Transaction_cptr>& transactions, const MinerId& reporter);

    // return transaction with commit block id if it exists in BlockTree, PendingTransactions or Database
    std::pair<Transaction_cptr, uint32_t> getTransaction(const TransactionId& transactionId) const;

    // return transactions if they exist in BlockTree, PendingTransactions or Database, otherwise has nullptr in vector
    std::vector<Transaction_cptr> getTransactions(const std::vector<TransactionId>& transactions) const;

    // returns debug info
    json getDebugInfo() const;

    BlockTree& getBlockTree()
    {
        return *m_blockTree;
    }

    PendingTransactions& getPendingTransactions()
    {
        return *m_pendingTransactions;
    }

    BlockchainOptions getOptions() const
    {
        return m_options;
    }

    uint64_t getInitializationTime() const
    {
        return m_initializationTime;
    }

    uint32_t getBlockInterval() const
    {
        return kBlockInterval;
    }

    MinerId getMinerId() const
    {
        return MinerId(m_options.minerKey.publicKey());
    }

    PublicKey getMinerPublicKey() const
    {
        return m_options.minerKey.publicKey();
    }

    PrivateKey getMinerKey() const
    {
        return m_options.minerKey;
    }

    //
    virtual bool canMineBlocks() const
    {
        return true;
    }
    // 
    virtual uint32_t getLatestBlockId() const;
    //
    virtual uint32_t getExpectedBlockId() const;
    // 
    uint32_t getPendingExecutionBlockId() const;
    //
    bool isDesynchronized() const;
    
    // calculate next miners
    static MinersQueue getNextMiners(const MinersQueue& currentMinersQueue,
                                     const TopMinersSet& topMiners, uint32_t newMiners, uint32_t currentBlockId = 0);

protected:
    // load blockchain state from database
    void load();
    // inits new blockchain
    void init();

    // checks mining process and processes transactions
    virtual void check();
    // updates blockchain
    void update();
    // update branch, returns true if blockchain has been updated
    bool updateBranch();
    // checks mining process, producess new block if it's correct time, returns true if new block has been created
    bool checkMining();
    // checks transactions
    void checkTransactions();
    // executes again valid transaction from removed block
    void recoverTransactions(const std::vector<Block_cptr>& blocks);
    // executes pending transactions
    void processPendingTransactions(uint32_t blockId, const chrono::high_resolution_clock::time_point& deadline,
                                    size_t maxTransactions = 0, size_t maxTransactionsSize = 0);

    // checks if transaction can be executed
    PostTransactionResult canExecuteTransaction(const Transaction_cptr& transaction, uint32_t blockId);
    // executes transaction
    PostTransactionResult executeTransaction(const Transaction_cptr& transaction, uint32_t blockId,
                                             bool isCryptoVerified = false);

    // minees first block with signed init transaction
    Block_cptr mineFirstBlock() const;
    // minees new block
    Block_cptr mineBlock(uint32_t blockId, const chrono::high_resolution_clock::time_point& deadline);
    // creates block
    Block_cptr createBlock(uint32_t blockId, std::vector<Transaction_cptr> transactions, const PrivateKey& key) const;

    // validate block, executes every transaction from block, then add it, return true if block has been added
    bool addBlock(const Block_cptr& block, bool ignoreTime = false);

protected:
    const BlockchainOptions m_options;
    const std::shared_ptr<Database> m_database;
    mutable Logger m_logger;

    SharedThread<Events> m_events;
    std::shared_ptr<CryptoVerifier> m_verifier;
    std::shared_ptr<Bans> m_bans;
    std::shared_ptr<PendingTransactions> m_pendingTransactions;
    std::shared_ptr<BlockTree> m_blockTree;

private:
    uint64_t m_initializationTime = 0;
    uint32_t m_lastMinedBlockId = 0;
    chrono::steady_clock::time_point m_lastMiningTime;
    chrono::steady_clock::time_point m_lastUpdate;

    boost::asio::steady_timer m_timer;
};

}
