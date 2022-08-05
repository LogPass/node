#include "pch.h"

#include "blockchain.h"
#include "events.h"
#include "transactions/init.h"
#include "transactions/commit.h"

namespace logpass {

Blockchain::Blockchain(const BlockchainOptions& options, const std::shared_ptr<Database>& database) :
    EventLoopThread("blockchain"), m_options(options), m_database(database), m_timer(m_context)
{
    ASSERT(m_database);
    ASSERT(m_options.minerKey.isValid());

    m_logger.setClassName("Blockchain");
    m_logger.setId(getMinerId().toString());
}

Blockchain::~Blockchain()
{

}

void Blockchain::start()
{
    m_verifier = std::make_shared<CryptoVerifier>(m_options.threads);
    m_bans = std::make_shared<Bans>();
    m_pendingTransactions = std::make_shared<PendingTransactions>();
    m_blockTree = std::make_shared<BlockTree>(m_bans, m_pendingTransactions);

    std::promise<void> f;
    post([this, &f] {
        ASSERT(std::this_thread::get_id() == m_thread.get_id());

        if (!m_database->confirmed().blocks.getLatestBlockHeader()) {
            init();
        }

        load();

        LOG_CLASS(info) << "Starting blockchain";
        f.set_value();
        check();
    });
    EventLoopThread::start();
    f.get_future().get();
}

void Blockchain::stop()
{
    post([this] {
        m_timer.cancel();
        m_blockTree = nullptr;
        m_pendingTransactions = nullptr;
        m_bans = nullptr;
        m_verifier->stop();
        m_verifier = nullptr;
    });
    EventLoopThread::stop();
}

std::unique_ptr<EventsListener> Blockchain::registerEventsListener(EventsListenerCallbacks&& callbacks) noexcept
{
    return std::make_unique<EventsListener>(m_events, std::move(callbacks));
}

void Blockchain::postTransaction(Serializer& serializer, PostTransactionCallback&& cb) noexcept
{
    try {
        auto transaction = Transaction::load(serializer);
        if (!serializer.eof()) {
            THROW_SERIALIZER_EXCEPTION("Unknown extra transaction data");
        }
        return postTransaction(transaction, std::move(cb));
    } catch (const SerializerException& e) {
        return cb(PostTransactionResult(PostTransactionResult::Status::SERIALIZER_ERROR, e.what()));
    }
}

void Blockchain::postTransaction(const Serializer_ptr& serializer, PostTransactionCallback&& cb) noexcept
{
    ASSERT(serializer);
    return postTransaction(*serializer, std::move(cb));
}

void Blockchain::postTransaction(const Transaction_cptr& transaction, PostTransactionCallback&& cb) noexcept
{
    post([this, transaction, cb = std::move(cb)]() mutable {
        ASSERT(std::this_thread::get_id() == m_thread.get_id());
        LOG_CLASS(trace) << "Posting transaction: " << transaction->getId();

        if (isStopped()) {
            return cb(PostTransactionResult(transaction->getId(), PostTransactionResult::Status::TIMEOUT));
        }

        if (isDesynchronized()) {
            return cb(PostTransactionResult(transaction->getId(), PostTransactionResult::Status::DESYNCHRONIZED));
        }

        // check is transaction is already pending and executed
        if (m_pendingTransactions->hasExecutedTransaction(transaction->getId())) {
            return cb(PostTransactionResult(transaction->getId(), PostTransactionResult::Status::SUCCESS));
        }

        // add transaction to pending queue if it's requested by some pending block
        m_pendingTransactions->addTransactionIfRequested(transaction);

        if (!m_pendingTransactions->canAddTransaction(transaction->getId())) {
            return cb(PostTransactionResult(transaction->getId(),
                                            PostTransactionResult::Status::REACHED_PENDING_LIMIT));
        }

        PostTransactionResult result = canExecuteTransaction(transaction, getPendingExecutionBlockId());
        if (!result) {
            return cb(std::move(result));
        }

        // do crypto verification on other thread
        m_verifier->verify(transaction, (TransactionVerifyCallback)
                           [this, transaction, cb = std::move(cb), weakSelf = weak_from_this()](auto result) mutable {
            ASSERT(std::this_thread::get_id() != m_thread.get_id());
            auto self = weakSelf.lock();
            if (!self) {
                return cb(PostTransactionResult(transaction->getId(), PostTransactionResult::Status::TIMEOUT));
            }

            if (!result || *result == false) {
                return cb(PostTransactionResult(transaction->getId(), PostTransactionResult::Status::SIGNATURE_ERROR));
            }

            post([this, transaction, cb = std::move(cb)]() {
                ASSERT(std::this_thread::get_id() == m_thread.get_id());
                if (isStopped()) {
                    return cb(PostTransactionResult(transaction->getId(), PostTransactionResult::Status::TIMEOUT));
                }

                if (!m_pendingTransactions->canAddTransaction(transaction->getId())) {
                    return cb(PostTransactionResult(transaction->getId(),
                                                    PostTransactionResult::Status::REACHED_PENDING_LIMIT));
                }

                return cb(PostTransactionResult(executeTransaction(transaction, getPendingExecutionBlockId(), true)));
            });
        });
    });
}

size_t Blockchain::addTransactions(const std::vector<Transaction_cptr>& transactions, const MinerId& reporter)
{
    // there's no need to move to main thread, bellow functions are thread-safe
    return m_pendingTransactions->addTransactions(transactions, reporter);
}

std::pair<Transaction_cptr, uint32_t> Blockchain::getTransaction(const TransactionId& transactionId) const
{
    // thread-safe
    auto activeBranch = m_blockTree->getActiveBranch();
    for (auto& node : activeBranch) {
        auto transaction = node.block->getTransaction(transactionId);
        if (transaction) {
            return { transaction, node.getId() };
        }
    }

    auto [transaction, blockId] = m_database->unconfirmed().transactions.getTransactionWithBlockId(transactionId);
    if (transaction) {
        return { transaction, blockId };
    }

    return { m_pendingTransactions->getTransaction(transactionId), 0 };
}

std::vector<Transaction_cptr> Blockchain::getTransactions(const std::vector<TransactionId>& transactions) const
{
    // thread-safe
    std::vector<Transaction_cptr> ret;
    auto activeBranch = m_blockTree->getActiveBranch();
    for (auto& transactionId : transactions) {
        auto transaction = m_pendingTransactions->getTransaction(transactionId);
        if (!transaction) {
            for (auto& node : activeBranch) {
                transaction = node.block->getTransaction(transactionId);
                if (transaction) {
                    break;
                }
            }
            if (!transaction) {
                transaction = m_database->unconfirmed().transactions.getTransaction(transactionId);
            }
        }
        ret.push_back(transaction);
    }
    return ret;
}

json Blockchain::getDebugInfo() const
{
    // thread-safe functions
    return {
        {"initialization_time", getInitializationTime()},
        {"last_block_id", getLatestBlockId()},
        {"expected_block_id", getExpectedBlockId()},
        {"pending_execution_block_id", getPendingExecutionBlockId()},
        {"pending_transactions", m_pendingTransactions->getDebugInfo() },
        {"block_tree", m_blockTree->getDebugInfo()}
    };
}

uint32_t Blockchain::getLatestBlockId() const
{
    return m_database->confirmed().blocks.getLatestBlockId();
}

uint32_t Blockchain::getExpectedBlockId() const
{
    ASSERT(m_initializationTime != 0);
    return (uint32_t)(((size_t)time(nullptr) - getInitializationTime()) / getBlockInterval());
}

uint32_t Blockchain::getPendingExecutionBlockId() const
{
    if (isDesynchronized()) {
        return getLatestBlockId() + kMinersQueueSize / 2;
    }
    return std::max<uint32_t>(getExpectedBlockId(), getLatestBlockId() + 1);
}

bool Blockchain::isDesynchronized() const
{
    return (getLatestBlockId() + kMinersQueueSize / 2) < getExpectedBlockId();
}

void Blockchain::load()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    LOG_CLASS(info) << "Loading blockchain";

    auto latestBlocks = m_database->confirmed().blocks.getLatestBlocks();
    if (latestBlocks.empty()) {
        LOG_CLASS(fatal) << "Can't load latest block headers from database, probably blockchain is not initialized";
        std::terminate();
    }

    auto firstBlocksTransactions = m_database->confirmed().blocks.getBlockTransactionIds(1, 0);
    if (!firstBlocksTransactions || firstBlocksTransactions->size() != 1) {
        LOG_CLASS(fatal) << "Invalid first block, doesn't exist or does have invalid number of transactions";
        std::terminate();
    }

    auto firstTransaction = m_database->confirmed().transactions.getTransaction(firstBlocksTransactions->at(0));
    if (!firstTransaction || firstTransaction->getType() != InitTransaction::TYPE) {
        LOG_CLASS(fatal) << "First transaction doesn't exists or is invalid";
        std::terminate();
    }

    auto initTransaction = std::static_pointer_cast<const InitTransaction>(firstTransaction);
    if (!initTransaction) {
        LOG_CLASS(fatal) << "Can't load init transaction from first block";
        std::terminate();
    }

    m_initializationTime = initTransaction->getInitializationTime();

    if (initTransaction->getBlockInterval() != getBlockInterval()) {
        LOG_CLASS(fatal) << "Invalid block intrerval. First transaction has block interval " <<
            initTransaction->getBlockInterval() << ", but block interval in config is set to " <<
            getBlockInterval();
        std::terminate();
    }

    size_t maxBlocks = std::min<size_t>(kDatabaseRolbackableBlocks + 1, (size_t)m_database->getMaxRollbackDepth() + 1);

    std::deque<Block_cptr> blocks;
    for (auto& [blockId, blockBodyAndHeader] : latestBlocks | views::reverse) {
        LOG_CLASS(debug) << "Loading block " << blockId;
        if (m_options.firstBlocks.contains(blockId)) {
            blocks.push_front(m_options.firstBlocks.find(blockId)->second);
        } else {
            blocks.push_front(m_database->confirmed().blocks.getBlock(blockId));
        }
        ASSERT(blocks.back() != nullptr);
        if (blocks.size() == maxBlocks) {
            break;
        }
    }

    MinersQueue miningQueue;
    for (auto& [blockId, blockBodyAndHeader] : latestBlocks) {
        auto nextMiners = blockBodyAndHeader.first->getNextMiners();
        miningQueue.insert(miningQueue.end(), nextMiners.begin(), nextMiners.end());
        if (blockId == blocks.front()->getId()) {
            break;
        }
    }
    if (miningQueue.size() > kMinersQueueSize) {
        miningQueue.erase(miningQueue.begin(), std::next(miningQueue.begin(), miningQueue.size() - kMinersQueueSize));
    }

    m_blockTree->loadBlocks(blocks, miningQueue);
    ASSERT(m_blockTree->getActiveBranch().back().getId() == m_database->confirmed().blocks.getLatestBlockId());
    // don't mine new block right after start, node is probably desynchronized
    m_lastUpdate = chrono::steady_clock::now();
    m_lastMiningTime = chrono::steady_clock::now();
}

void Blockchain::init()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    LOG_CLASS(info) << "Initializing blockchain";

    if (!m_options.firstBlocks.empty()) {
        for (auto& [blockId, block] : m_options.firstBlocks) {
            if (!addBlock(block)) {
                LOG_CLASS(fatal) << "Can not initialize blockchain with provided first blocks (they are invalid)";
                std::terminate();
            }
        }
        return;
    }

    if (!m_options.initialize) {
        LOG_CLASS(fatal) << "Blockchain is not initialized and options do not allow to initialize it";
        std::terminate();
    }

    Block_cptr firstBlocks = mineFirstBlock();
    if (!addBlock(firstBlocks)) {
        LOG_CLASS(fatal) << "Initialization fatal error, first block can not be added";
        std::terminate();
    }
}

void Blockchain::check()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());

    // call check every 100 ms
    m_timer.expires_after(chrono::milliseconds(100));
    m_timer.async_wait([this](auto ec) { if (!ec && !isStopped()) { return check(); }});

    // update block tree
    update();
}

void Blockchain::update()
{
    if (updateBranch()) {
        return;
    }
    if (checkMining()) {
        return;
    }

    checkTransactions();
}

bool Blockchain::checkMining()
{
    if (!canMineBlocks()) {
        return false;
    }

    // calculate current block id
    uint32_t latestBlockId = getLatestBlockId();
    uint32_t expectedBlockId = getExpectedBlockId();
    if (latestBlockId >= expectedBlockId) {
        return false; // up to date, there's nothing to be done
    }
    if (m_lastMinedBlockId >= expectedBlockId) {
        return false; // never mine the same block twice (in case of desynchronization)
    }
    if (m_lastMiningTime + chrono::seconds(kBlockInterval / 2) >= chrono::steady_clock::now()) {
        return false; // avoid mining too often
    }

    auto activeBranch = m_blockTree->getActiveBranch();
    auto miningQueue = m_database->confirmed().blocks.getMinersQueue();

    size_t lastDifferentMinerIndex = 0;
    for (size_t i = 0; i < activeBranch.size(); ++i) {
        if (activeBranch[i].getMinerId() != getMinerId()) {
            lastDifferentMinerIndex = i;
        }
    }

    uint32_t otherMinersInQueue = miningQueue.size() -
        std::count(miningQueue.begin(), miningQueue.end(), getMinerId());
    bool isProbablyDesynchronized = lastDifferentMinerIndex < kDatabaseRolbackableBlocks / 2 &&
        activeBranch[0].getId() + kDatabaseRolbackableBlocks / 2 < expectedBlockId &&
        otherMinersInQueue >= miningQueue.size() * 8 / 10;

    if (isProbablyDesynchronized && miningQueue.front() != getMinerId()) {
        LOG_CLASS(warning) << "Skipping block producation, there are too many missing blocks from other nodes. " <<
            "It may be a network issue, check connection to other nodes.";
        m_lastMiningTime = chrono::steady_clock::now();
        return false;
    }

    uint32_t miningQueueIndex = expectedBlockId - latestBlockId - 1;
    if (miningQueueIndex >= miningQueue.size()) {
        // if there is no new block, and miner is in last 16 miners, just mine new block
        if (m_lastUpdate + chrono::seconds(kBlockInterval) > chrono::steady_clock::now()) {
            return false; // there is no need to mine new block, we just got one
        }

        uint32_t ourMiningTurn = 0;
        for (uint32_t i = miningQueue.size() - 16; i < miningQueue.size(); ++i) {
            if (miningQueue[i] == getMinerId()) {
                ourMiningTurn = i;
            }
        }

        if (ourMiningTurn == 0) {
            return false;
        }

        miningQueueIndex = ourMiningTurn;
    } else if (miningQueue[miningQueueIndex] != getMinerId()) {
        return false; // not our turn
    }

    m_lastMiningTime = chrono::steady_clock::now();
    m_lastMinedBlockId = latestBlockId + miningQueueIndex + 1;
    Block_cptr newBlock = mineBlock(latestBlockId + miningQueueIndex + 1,
        chrono::high_resolution_clock::now() + chrono::seconds(2));
    m_blockTree->addBlock(newBlock, MinerId());

    if (newBlock->getId() == getExpectedBlockId() && time(nullptr) % kBlockInterval == 0) {
        // if block was generated too fast, wait 1 second in case of desynchronized clocks
        LOG_CLASS(info) << "Block " << newBlock->getId() << " was generated very fast, waiting 1s";
        m_timer.expires_after(chrono::seconds(1));
        m_timer.async_wait([this](auto ec) { if (!ec && !isStopped()) { return check(); }});
        return true;
    }

    return updateBranch();
}

void Blockchain::checkTransactions()
{
    processPendingTransactions(getPendingExecutionBlockId(),
        chrono::high_resolution_clock::now() + chrono::seconds(1));
}

bool Blockchain::updateBranch()
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());

    auto activeBranch = m_blockTree->getActiveBranch();
    auto longestBranch = m_blockTree->getLongestBranch();

    if (activeBranch.size() == longestBranch.size() &&
        activeBranch.back().getHeaderHash() == longestBranch.back().getHeaderHash()) {
        return false;
    }

    ASSERT(longestBranch.size() >= activeBranch.size());
    ASSERT(longestBranch.front().getHeaderHash() == activeBranch.front().getHeaderHash());
    ASSERT(activeBranch.back().getId() == m_database->confirmed().blocks.getLatestBlockId());

    // find first common parent
    size_t commonParentIndex = 0;
    for (size_t i = 0; i < activeBranch.size(); ++i) {
        if (activeBranch[i].getHeaderHash() != longestBranch[i].getHeaderHash()) {
            break;
        }
        commonParentIndex = i;
    }

    // rollback to parent if needed, load blocks if needed
    size_t blocksToRollback = activeBranch.size() - (commonParentIndex + 1);
    if (blocksToRollback > 0) {
        // do rollback
        if (!m_database->rollback(blocksToRollback)) {
            LOG_CLASS(fatal) << "Fatal error while rollbacking to common parent, can't do rollback by "
                << blocksToRollback << " blocks";
            std::terminate();
        }
        ASSERT(m_database->confirmed().blocks.getLatestBlockId() == activeBranch[commonParentIndex].getId());
    } else {
        // rollback only temporary changes
        m_database->clear();
    }

    // clear executed transactions
    m_pendingTransactions->clearExecutedTransactions();

    // execute new blocks
    bool success = true;
    size_t executedBlocks = 0;
    for (size_t i = commonParentIndex + 1; i < longestBranch.size(); ++i) {
        if (!addBlock(longestBranch[i].block)) {
            LOG_CLASS(info) << "Invalid block " << longestBranch[i].toString();
            success = false;
            break;
        }
        executedBlocks += 1;
    }

    // restore old branch in case of error
    if (!success) {
        if (executedBlocks > 0) {
            if (!m_database->rollback(executedBlocks)) {
                LOG_CLASS(fatal) << "Fatal error while restoring old branch, can't do rollback by "
                    << executedBlocks << " blocks";
                std::terminate();
            }
        }
        // restore old branch
        for (size_t i = commonParentIndex + 1; i < activeBranch.size(); ++i) {
            if (!addBlock(activeBranch[i].block)) {
                LOG_CLASS(fatal) << "Fatal error, can't restore old branch";
                std::terminate();
            }
        }
        // ban invalid block and his repoter
        m_blockTree->banBlock(longestBranch[commonParentIndex + executedBlocks + 1].getHeaderHash(),
                              "execution error");
    } else { // execute callbacks
        ASSERT(m_database->confirmed().blocks.getLatestBlockId() == longestBranch.back().getId());
        m_blockTree->updateActiveBranch(longestBranch);
        m_lastUpdate = chrono::steady_clock::now();
        std::vector<Block_cptr> blocks;
        for (size_t i = commonParentIndex + 1; i < longestBranch.size(); ++i) {
            blocks.push_back(longestBranch[i].block);
        }
        m_events->onBlocks(blocks, blocksToRollback > 0);

        // recover transactions from removed blocks
        std::vector<Block_cptr> blocksToRecover;
        for (size_t i = commonParentIndex + 1; i < activeBranch.size(); ++i) {
            blocksToRecover.push_back(activeBranch[i].block);
        }
        if (!blocksToRecover.empty()) {
            recoverTransactions(blocksToRecover);
        }
    }

    // inform about pending transactions
    LOG_CLASS(info) << "There are " << m_pendingTransactions->getPendingTransactionsCount() <<
        " pending transactions with size of " << (m_pendingTransactions->getPendingTransactionsSize() / 1024) <<
        " KBs";

    return true;
}

void Blockchain::recoverTransactions(const std::vector<Block_cptr>& blocks)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    ASSERT(m_pendingTransactions->getExecutedTransactionsCount() == 0);

    for (auto& block : blocks) {
        LOG_CLASS(debug) << "recovering " << block->getTransactions() << " transaction from block " << block->getId();
        std::vector<Transaction_cptr> transactions;
        transactions.reserve(block->getTransactions());
        for (auto transaction : *block) {
            if (!transaction->getTransactionSettings().isBlockchainManagementTransaction)
                transactions.push_back(transaction);
        }
        // add transactions as executed, because they're crypto verified, later clear executed transactions to move them
        // to the beginning of pending transactions queue
        m_pendingTransactions->addExecutedTransactions(transactions);
    }
    m_pendingTransactions->clearExecutedTransactions();
}

void Blockchain::processPendingTransactions(uint32_t blockId,
                                            const chrono::high_resolution_clock::time_point& deadline,
                                            size_t maxTransactions, size_t maxTransactionsSize)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());

    while (chrono::high_resolution_clock::now() < deadline) {
        auto pendingTransactions = m_pendingTransactions->getPendingTransactions(128);
        if (pendingTransactions.empty()) {
            break;
        }

        std::set<TransactionId> invalidTransactions;
        std::vector<Transaction_cptr> prevalidatedTransactions;
        for (auto& transaction : pendingTransactions) {
            if (!canExecuteTransaction(transaction, blockId)) {
                invalidTransactions.insert(transaction->getId());
                continue;                
            }
            prevalidatedTransactions.push_back(transaction);
        }


        std::vector<Transaction_cptr> transactionsToVerify;
        for (auto& transaction : prevalidatedTransactions) {
            if (!m_pendingTransactions->isTransactionCryptoVerified(transaction->getId())) {
                transactionsToVerify.push_back(transaction);
            }
        }

        auto results = m_verifier->verify(transactionsToVerify);
        for (size_t i = 0; i < results.size(); ++i) {
            if (results[i]) {
                m_pendingTransactions->setTransactionAsCryptoVerified(transactionsToVerify[i]->getId());
            } else {
                LOG_CLASS(warning) << "Transaction " << transactionsToVerify[i]->getId() << " has invalid signatures";
                invalidTransactions.insert(transactionsToVerify[i]->getId());
            }
        }


        LOG_CLASS(trace) << "executing " << prevalidatedTransactions.size() << " pending transactions for blockId " <<
            blockId;
        bool finished = false;
        for (auto& transaction : prevalidatedTransactions) {
            if (invalidTransactions.contains(transaction->getId())) {
                continue;
            }
            if (maxTransactions != 0 && m_pendingTransactions->getExecutedTransactionsCount() + 1 > maxTransactions) {
                finished = true;
                break;
            }
            if (maxTransactionsSize != 0 &&
                m_pendingTransactions->getExecutedTransactionsSize() + transaction->getSize() > maxTransactionsSize) {
                finished = true;
                break;
            }
            if(!executeTransaction(transaction, blockId)) {
                invalidTransactions.insert(transaction->getId());
            }
        }
        if (!invalidTransactions.empty()) {
            m_pendingTransactions->removeTransactions(invalidTransactions);
        }
        if (finished) {
            return;
        }
    }
}

// checks if transaction can be executed
PostTransactionResult Blockchain::canExecuteTransaction(const Transaction_cptr& transaction, uint32_t blockId)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());

    if (transaction->getTransactionSettings().isBlockchainManagementTransaction) {
        return PostTransactionResult(transaction->getId(), PostTransactionResult::Status::VALIDATION_ERROR,
                                     "This type of transaction can not be posted");
    }

    if (blockId >= transaction->getBlockId() + kTransactionMaxBlockIdDifference ||
        transaction->getBlockId() > blockId) {
        return PostTransactionResult(transaction->getId(), PostTransactionResult::Status::OUTDATED);
    }

    return PostTransactionResult(transaction->getId(), PostTransactionResult::Status::SUCCESS);
}

// executes transaction
PostTransactionResult Blockchain::executeTransaction(const Transaction_cptr& transaction, uint32_t blockId,
                                                     bool isCryptoVerified)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());

    auto result = canExecuteTransaction(transaction, blockId);
    if (!result) {
        return result;
    }

    if (!isCryptoVerified && !m_pendingTransactions->isTransactionCryptoVerified(transaction->getId()) &&
        !transaction->validateSignatures()) {
        return PostTransactionResult(transaction->getId(), PostTransactionResult::Status::SIGNATURE_ERROR);
    }

    UnconfirmedDatabase& database = m_database->unconfirmed();
    try {
        transaction->validate(blockId, database);
    } catch (const TransactionValidationError& e) {
        LOG_CLASS(debug) << "Transaction validation error in executeTransaction ("
            << transaction->getId() << "): " << e.what();
        return PostTransactionResult(transaction->getId(), PostTransactionResult::Status::VALIDATION_ERROR, e.what());
    }

    transaction->execute(blockId, database);

    bool isNewTransaction = m_pendingTransactions->addExecutedTransaction(transaction);
    if (isNewTransaction) {
        m_events->onNewTransactions({ transaction });
    }

    return PostTransactionResult(transaction->getId(), PostTransactionResult::Status::SUCCESS);
}


Block_cptr Blockchain::mineFirstBlock() const
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    PrivateKey key = m_options.minerKey;
    uint64_t initializationTime = (((size_t)time(nullptr) - 60) / 60) * 60;
    auto initTransaction = InitTransaction::create(1, 0, initializationTime, getBlockInterval());
    initTransaction->setUserId(UserId(key.publicKey()))->setPublicKey(key.publicKey())->sign({ key });
    return createBlock(1, { initTransaction }, key);
}

Block_cptr Blockchain::mineBlock(uint32_t blockId, const chrono::high_resolution_clock::time_point& deadline)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    ASSERT(blockId != 1);

    LOG_CLASS(info) << "Mining block " << blockId;
    m_database->clear();
    m_pendingTransactions->clearExecutedTransactions();

    processPendingTransactions(blockId, deadline, kBlockMaxTransactions - 1, kBlockMaxTransactionsSize - 1024);
    std::vector<Transaction_cptr> transactions = m_pendingTransactions->getExecutedTransactions(kBlockMaxTransactions);

    auto publicKey = getMinerKey().publicKey();
    auto minerId = MinerId(publicKey);
    auto miner = m_database->unconfirmed().miners.getMiner(minerId);
    if (!miner) {
        LOG_CLASS(warning) << "Can't create miner reward transaction, miner does not exists";
    } else {
        auto minerOwner = m_database->unconfirmed().users.getUser(miner->owner);
        if (minerOwner->hasKey(publicKey) == 1) {
            auto rewardTransaction = CommitTransaction::create(blockId, m_database->unconfirmed().state.getPricing(),
                                                               minerId, transactions.size(),
                                                               m_database->unconfirmed().users.getUsersCount(),
                                                               m_database->unconfirmed().users.getTokens(),
                                                               m_database->unconfirmed().miners.getStakedTokens());
            rewardTransaction->setUserId(miner->owner)->sign({ getMinerKey() });
            transactions.push_back(rewardTransaction);
        } else {
            LOG_CLASS(warning) << "Can't create miner reward transaction, miner key doesn't exist in owner keys";
        }
    }

    return createBlock(blockId, transactions, getMinerKey());
}

Block_cptr Blockchain::createBlock(uint32_t blockId, std::vector<Transaction_cptr> transactions,
                                   const PrivateKey& key) const
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    ASSERT(transactions.size() <= kBlockMaxTransactions);
    ASSERT(std::accumulate(transactions.begin(), transactions.end(), 0, [](auto val, auto& transaction) {
        return transaction->getSize();
    }) <= kBlockMaxTransactionsSize);
    LOG_CLASS(info) << "Creating block: " << blockId;

    auto lastBlockHeader = m_database->confirmed().blocks.getLatestBlockHeader();
    ASSERT((lastBlockHeader != nullptr) == (blockId != 1));
    ASSERT(!lastBlockHeader || blockId > lastBlockHeader->getId());

    uint32_t depth = lastBlockHeader ? lastBlockHeader->getDepth() + 1 : 1;
    auto prevHeaderHash = lastBlockHeader ? lastBlockHeader->getHash() : Hash();

    MinersQueue nextMiners;
    if (!lastBlockHeader) {
        nextMiners.resize(kMinersQueueSize, key.publicKey());
    } else {
        nextMiners = getNextMiners(m_database->confirmed().blocks.getMinersQueue(),
                                   m_database->confirmed().miners.getTopMiners(),
                                   blockId - lastBlockHeader->getId(), blockId);
    }

    return Block::create(blockId, depth, nextMiners, transactions, prevHeaderHash, key);
}

bool Blockchain::addBlock(const Block_cptr& block, bool ignoreTime)
{
    ASSERT(std::this_thread::get_id() == m_thread.get_id());
    LOG_CLASS(info) << "Adding block: " << block->toString();
    LOG_CLASS(info) << "Block has " << block->getTransactions() << " new transactions with size: " <<
        block->getTransactionsSize() / 1024 << " KB";

    auto addingStart = chrono::high_resolution_clock::now();
    BlockHeader_cptr lastBlock = m_database->confirmed().blocks.getLatestBlockHeader();
    MinersQueue miningQueue = m_database->confirmed().blocks.getMinersQueue();
    MinersQueue blockNextMiners = block->getBlockHeader()->getNextMiners();

    // validate block stucture, signature and next miners
    if (block->getId() == 1) {
        if (lastBlock || !miningQueue.empty()) {
            return false;
        }
        if (blockNextMiners.size() != kMinersQueueSize) {
            return false;
        }
        if (block->getDepth() != 1) {
            return false;
        }
        if (block->getTransactions() != 1) {
            return false;
        }
        auto firstTransaction = block->getTransaction(block->getTransactionId(0));
        for (MinerId expectedMinerId(firstTransaction->getMainKey()); auto & nextMiner : blockNextMiners) {
            if (nextMiner != expectedMinerId) {
                return false;
            }
        }
        if (!block->validate(block->getBlockHeader()->getNextMiners()[0], Hash())) {
            return false;
        }
    } else {
        if (!lastBlock) {
            return false;
        }
        if (!ignoreTime && block->getId() > getExpectedBlockId()) {
            LOG_CLASS(warning) << "Adding block failed, block (" << block->toString() <<
                ") is ahead of expected block id";
            return false;
        }
        if (block->getDepth() != lastBlock->getDepth() + 1) {
            LOG_CLASS(warning) << "Adding block failed, block (" << block->toString() << ") has invalid depth";
            return false;
        }
        if (lastBlock->getId() + block->getNextMiners().size() != block->getId()) {
            LOG_CLASS(warning) << "Adding block failed, invalid number of skipped blocks";
            return false;
        }
        if (!block->validate(miningQueue[block->getSkippedBlocks()], lastBlock->getHash())) {
            LOG_CLASS(warning) << "Adding block failed, block is invalid";
            return false;
        }
        auto nextMiners = block->getBlockHeader()->getNextMiners();
        auto correctNextMiners = getNextMiners(m_database->confirmed().blocks.getMinersQueue(),
                                               m_database->confirmed().miners.getTopMiners(),
                                               block->getSkippedBlocks() + 1, block->getId());
        if (nextMiners.size() != correctNextMiners.size()) {
            LOG_CLASS(warning) << "Adding block failed, next miners are invalid";
            return false;
        }
        for (size_t i = 0; i < nextMiners.size(); ++i) {
            if (nextMiners[i] != correctNextMiners[i]) {
                LOG_CLASS(warning) << "Adding block failed, next miners are invalid";
                return false;
            }
        }
    }
    // remove pending operations
    m_pendingTransactions->clearExecutedTransactions();
    m_database->clear();

    auto preloadThread = std::thread([&]() {
        SET_THREAD_NAME("preload");
        auto start = chrono::high_resolution_clock::now();
        std::set<UserId> userIds;
        for (auto transaction : *block) {
            m_database->unconfirmed().users.preloadUser(transaction->getUserId());
        }
        m_database->preload(block->getId());
        auto duration = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start);
        LOG_CLASS(info) << "Preloaded database in " << duration.count() << " ms.";
    });

    // crypto verify unverified transactions
    std::vector<Transaction_cptr> transactionsToVerify;
    for (auto transaction : *block) {
        if (!m_pendingTransactions->isTransactionCryptoVerified(transaction->getId())) {
            transactionsToVerify.push_back(transaction);
        }
    }

    if (transactionsToVerify.size() > 0) {
        auto verifyStart = chrono::high_resolution_clock::now();
        auto results = m_verifier->verify(transactionsToVerify);
        if (std::find(results.begin(), results.end(), false) != results.end()) {
            std::set<TransactionId> invalidTransactions;
            for (size_t i = 0; i < results.size(); ++i) {
                TransactionId transactionId = transactionsToVerify[i]->getId();
                if (results[i]) {
                    m_pendingTransactions->setTransactionAsCryptoVerified(transactionId);
                } else {
                    invalidTransactions.insert(transactionId);
                    LOG_CLASS(warning) << "Transaction " << transactionId << " has invalid signatures";
                }
            }
            m_pendingTransactions->removeTransactions(invalidTransactions);
            LOG_CLASS(warning) << "Block transactions crypto verification failed";
            return false;
        }
        auto duration = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - verifyStart);
        LOG_CLASS(info) << "Verified " << transactionsToVerify.size() << " transactions in "
            << duration.count() << " ms.";
    }

    // wait for preload thread
    preloadThread.join();

    // execute block
    auto executingStart = chrono::high_resolution_clock::now();
    size_t executedTransactionsSize = 0;
    std::set<TransactionId> executedTransactions;
    UnconfirmedDatabase& database = m_database->unconfirmed();
    for (auto transaction : *block) {
        try {
            transaction->validate(block->getId(), database);
        } catch (const TransactionValidationError& e) {
            LOG_CLASS(warning) << "Adding block failed, transaction validation error (" << transaction->getId() <<
                ": " << e.what() << ")";
            m_database->clear();
            return false;
        }

        transaction->execute(block->getId(), database);
        executedTransactionsSize += transaction->getSize();
        executedTransactions.insert(transaction->getId());
    }

    // add block
    database.blocks.addBlock(block);

    // asserations
    ASSERT(database.transactions.getNewTransactionsCount() == block->getBlockBody()->getTransactions());
    ASSERT(database.transactions.getNewTransactionsSize() == block->getBlockBody()->getTransactionsSize());

    // stats
    auto now = chrono::high_resolution_clock::now();
    LOG_CLASS(info) << "Executed block in " <<
        chrono::duration_cast<chrono::milliseconds>(now - executingStart).count() << " ms.";
    LOG_CLASS(info) << "Added block in " <<
        chrono::duration_cast<chrono::milliseconds>(now - addingStart).count() << " ms.";

    // commit changes
    auto commitStart = chrono::high_resolution_clock::now();
    m_database->commit(block->getId());
    now = chrono::high_resolution_clock::now();
    LOG_CLASS(info) << "Committed block in " <<
        chrono::duration_cast<chrono::milliseconds>(now - commitStart).count() << " ms.";

    // remove executed transactions from pending transactions
    m_pendingTransactions->removeTransactions(executedTransactions);

    return true;
}

MinersQueue Blockchain::getNextMiners(const MinersQueue& currentMinersQueue,
                                      const TopMinersSet& topMiners, uint32_t newMiners, uint32_t currentBlockId)
{
    ASSERT(newMiners <= kMinersQueueSize &&
           (currentMinersQueue.empty() || currentMinersQueue.size() == kMinersQueueSize));

    // calculate minimum stake and potential miners
    size_t stakesSum = 0;
    std::vector<Miner_cptr> potentialMiners;
    for (auto& miner : topMiners) {
        size_t minerStake = std::max<size_t>(1, miner->getActiveStake(currentBlockId));
        stakesSum += minerStake;
        size_t miningPeriod = (stakesSum + minerStake - 1ull) / minerStake;
        if (miningPeriod > kMinersQueueSize) {
            stakesSum -= minerStake;
            break;
        }
        potentialMiners.push_back(miner);
    }

    // calculate mining period and current distance of selected miners
    std::map<MinerId, int16_t> lastMining; // miner - distance    
    for (size_t distance = currentMinersQueue.size(); auto & miner : currentMinersQueue) {
        lastMining[miner] = (int16_t)distance;
        distance -= 1;
    }

    struct SelectedMiner {
        Miner_cptr miner;
        int16_t period;
        int16_t distance;
    };
    std::vector<SelectedMiner> selectedMiners;

    for (auto& miner : potentialMiners) {
        size_t minerStake = miner->getActiveStake(currentBlockId);
        int16_t miningPeriod = (int16_t)std::ceil((double)stakesSum / (double)minerStake);
        int16_t lastMiningDistance = kMinersQueueSize;
        auto it = lastMining.find(miner->id);
        if (it != lastMining.end()) {
            lastMiningDistance = it->second;
        }
        selectedMiners.push_back({ miner, miningPeriod, lastMiningDistance });
    }

    // calculate next miners
    MinersQueue nextMiners;
    while (nextMiners.size() < newMiners) {
        for (auto& selectedMiner : selectedMiners) {
            if (selectedMiner.distance >= selectedMiner.period) {
                nextMiners.push_back(selectedMiner.miner->id);
                if (nextMiners.size() == newMiners) {
                    break;
                }
                selectedMiner.distance = 0;
            }
            selectedMiner.distance += 1;
        }
    }

    return nextMiners;
}

}
