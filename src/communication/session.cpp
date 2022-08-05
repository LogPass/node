#include "pch.h"

#include "session.h"
#include "packets/first_packet.h"
#include "packets/get_block_header.h"
#include "packets/get_block.h"
#include "packets/get_new_transactions.h"
#include "packets/new_blocks.h"
#include "packets/new_transactions.h"

namespace logpass {

Session::Session(const MinerId& minerId, const std::shared_ptr<Blockchain>& blockchain,
                 const std::shared_ptr<const Database>& database) :
    m_minerId(minerId), m_blockchain(blockchain), m_database(database)
{
    ASSERT(m_minerId.isValid() && m_blockchain != nullptr && m_database != nullptr);
    m_logger.add_attribute("Class", boost::log::attributes::constant<std::string>("Session"));
    m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(minerId.toString()));
}

void Session::onConnected(Connection* connection, const BlockHeader_cptr& latestBlockHeader)
{
    ASSERT(!m_connection && connection && connection->getMinerId() == m_minerId);
    m_connection = connection;
    m_firstPacket = true;
    m_latestBlockHeader = latestBlockHeader;

    // create and send first packet
    auto firstPacket = FirstPacket::create(latestBlockHeader);
    m_connection->send(firstPacket);
}

void Session::onDisconnected()
{
    ASSERT(m_connection != nullptr);
    m_connection = nullptr;
}

bool Session::onPacket(const Packet_ptr& packet)
{
    ASSERT(m_connection != nullptr);
    if (m_firstPacket != (packet->getType() == FirstPacket::TYPE)) {
        LOG_CLASS(warning) << "invalid first packet " << (uint16_t)packet->getType();
        return false;
    }

    // execute packet
    try {
        packet->execute({
                            .database = &(m_database->confirmed()),
                            .blockchain = m_blockchain.get(),
                            .blockTree = &(m_blockchain->getBlockTree()),
                            .connection = m_connection,
                            .session = this,
                            .sessionData = &m_data
                        });
    } catch (PacketExecutionException& e) {
        LOG_CLASS(warning) << "packet execution exception: " << e.what();
        return false;
    }

    if (packet->getType() == FirstPacket::TYPE) {
        m_firstPacket = false;
    }

    return true;
}

void Session::onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch)
{
    ASSERT(m_connection != nullptr);
    LOG_CLASS(trace) << "onBlocks " << blocks.size() << " (" << (didChangeBranch ? "new" : "old") << " branch) " <<
        "last block: " << blocks.back()->toString();

    m_latestBlockHeader = blocks.back()->getBlockHeader();

    auto packet = NewBlocksPacket::create(m_latestBlockHeader);
    m_connection->send(packet);

    if (!m_data.lastBlockHeader) {
        return;
    }

    if (m_data.waitingForNewBlock) {
        if (m_blockchain->getBlockTree().isInLastLevel(m_data.lastRecivedBlockHash)) {
            return;
        }
        m_data.waitingForNewBlock = false;
        if (!m_data.requestingBlock) {
            requestBlockHeader();
        }
    }

    if (!m_data.sharedPendingTransactions && m_latestBlockHeader->getDepth() == m_data.lastBlockHeader->getDepth()) {
        sendFirstPendingTransactions();
    }

    if (!m_data.requestingTransactions && !m_data.recivedNewTransactionIds.empty()) {
        requestNewTransactions();
    }
}

void Session::onNewTransactions(const std::vector<Transaction_cptr>& transactions)
{
    ASSERT(m_connection != nullptr);
    LOG_CLASS(trace) << "onPendingTransactions " << transactions.size();

    if (!m_data.sharedPendingTransactions) {
        return;
    }

    for (auto& transaction : transactions) {
        if (!m_sharedTransactionIds.addTransactionId(transaction->getId())) {
            continue;
        }

        m_data.newTransactionIds.insert(transaction->getId());
        if (m_data.newTransactionIds.size() == NewTransactionsPacket::MAX_TRANSACTION_IDS) {
            sendNewTransactionIds(m_data.newTransactionIds);
            m_data.newTransactionIds.clear();
        }
    }
}

void Session::onPeriodicalCheck()
{
    ASSERT(m_connection != nullptr);
    if (m_firstPacket) {
        return;
    }

    if (!m_data.lastBlockHeader) {
        return;
    }

    if (m_latestBlockHeader->getDepth() >= m_data.lastBlockHeader->getDepth()) {
        if (m_data.lastBlockHeaderTime + chrono::seconds(kBlockInterval * 4) < chrono::steady_clock::now()) {
            // close connection if there was no new block header reported during last kBlockInterval * 4 seconds
            THROW_EXCEPTION(SessionException("timeout for new block header"));
        }
    }

    if (m_data.requestingBlock) {
        return;
    }

    if (!m_data.sharedPendingTransactions) {
        return;
    }

    if (!m_data.newTransactionIds.empty()) {
        sendNewTransactionIds(m_data.newTransactionIds);
        m_data.newTransactionIds.clear();
    }
}

void Session::onFirstPacket(const BlockHeader_cptr& lastBlockHeader)
{
    LOG_CLASS(trace) << "onFirstPacket " << lastBlockHeader->toString();

    m_data.lastBlockHeader = lastBlockHeader;
    m_data.lastBlockHeaderTime = chrono::steady_clock::now();

    if (m_latestBlockHeader->getDepth() == m_data.lastBlockHeader->getDepth()) {
        sendFirstPendingTransactions();
    }

    if (m_latestBlockHeader->getDepth() >= m_data.lastBlockHeader->getDepth()) {
        return;
    }

    return requestBlockHeader();
}

void Session::onNewBlocks(const BlockHeader_cptr& lastBlockHeader)
{
    LOG_CLASS(debug) << "onNewBlocks (" << lastBlockHeader->toString() << ")";

    if (lastBlockHeader->getDepth() <= m_data.lastBlockHeader->getDepth()) {
        LOG_CLASS(warning) << "invalid new block: " << m_data.lastBlockHeader->toString() << ", previous block: " <<
            lastBlockHeader->toString();
        THROW_EXCEPTION(SessionException("wrong number of blocks"));
        return;
    }

    m_data.lastBlockHeader = lastBlockHeader;
    m_data.lastBlockHeaderTime = chrono::steady_clock::now();

    if (!m_data.sharedPendingTransactions && m_latestBlockHeader->getDepth() == m_data.lastBlockHeader->getDepth()) {
        sendFirstPendingTransactions();
    }

    if (m_latestBlockHeader->getDepth() >= m_data.lastBlockHeader->getDepth()) {
        return;
    }

    if (m_data.requestingBlock) {
        return;
    }

    if (m_blockchain->getBlockTree().isInLastLevel(m_data.lastRecivedBlockHash)) {
        return;
    }

    requestBlockHeader();
}

void Session::onNewTransactionIds(const std::set<TransactionId>& transactionIds)
{
    LOG_CLASS(trace) << "onNewTransactionIds " << transactionIds.size();

    m_data.recivedNewTransactionIds.insert(transactionIds.begin(), transactionIds.end());
    if (!m_data.requestingTransactions && !m_data.recivedNewTransactionIds.empty())
        requestNewTransactions();
}

void Session::onTransactions(const std::map<TransactionId, Transaction_cptr>& transactionsMap)
{
    ASSERT(m_data.requestingTransactions);
    LOG_CLASS(trace) << "onTransactions " << transactionsMap.size();

    m_data.requestingTransactions = false;
    std::vector<Transaction_cptr> transactions;
    for (auto& [transactionId, transaction] : transactionsMap) {
        if (!transaction)
            continue;
        transactions.push_back(transaction);
    }
    m_blockchain->addTransactions({ transactions }, m_minerId);

    if (!m_data.recivedNewTransactionIds.empty())
        requestNewTransactions();
}

void Session::onBlockHeader(const BlockHeader_cptr& header)
{
    ASSERT(m_data.requestingBlock);

    m_data.requestingBlock = false;

    if (!header) {
        LOG_CLASS(warning) << "onBlockHeader, missing block header";
        THROW_EXCEPTION(SessionException("missing block header"));
        return;
    }

    LOG_CLASS(debug) << "onBlockHeader " << header->toString();

    if (header->getId() > m_blockchain->getExpectedBlockId()) {
        LOG_CLASS(warning) << "recived block header (" << header->toString() << ") is ahead of expected block id (" <<
            m_blockchain->getExpectedBlockId() << ")";
        THROW_EXCEPTION(SessionException("invalid block header (ahead of time)"));
        return;
    }

    auto [pendingBlock, isDuplicated] = m_blockchain->getBlockTree().addBlockHeader(header, m_minerId);
    if (pendingBlock) {
        requestBlock(pendingBlock);
        return;
    }

    if (isDuplicated) {
        // pending block has been already converted to block
        LOG_CLASS(debug) << "Duplicated block header " << header->toString();
        if (m_blockchain->getBlockTree().isInLastLevel(header->getHash())) {
            // block is waiting for execution
            LOG_CLASS(debug) << "Waiting for new block, current is in last level";
            m_data.waitingForNewBlock = true;
            m_data.lastRecivedBlockHash = header->getHash();
            return;
        }

        // get next block
        requestBlockHeader();
        return;
    }

    LOG_CLASS(warning) << "Invalid block header " << header->toString();
    THROW_EXCEPTION(SessionException("invalid block header"));
}

void Session::onCompletedBlock(const PendingBlock_ptr& pendingBlock)
{
    ASSERT(m_data.requestingBlock);
    LOG_CLASS(trace) << "Completed block (" << pendingBlock->toString() << ")";
    m_data.requestingBlock = false;

    if (pendingBlock->getId() >= m_data.lastBlockHeader->getId()) {
        LOG_CLASS(debug) << "There's no newer block";
        return; // there's no newer block
    }

    if (m_blockchain->getBlockTree().isInLastLevel(pendingBlock->getHeaderHash())) {
        LOG_CLASS(debug) << "Waiting for new block, current is in last level";
        m_data.waitingForNewBlock = true;
        m_data.lastRecivedBlockHash = pendingBlock->getHeaderHash();
        return; // block awaits execution
    }

    // request next block
    requestBlockHeader();
}

void Session::onInvalidBlock(const PendingBlock_ptr& pendingBlock)
{
    ASSERT(m_data.requestingBlock);
    m_data.requestingBlock = false;

    LOG_CLASS(warning) << "Invalid block";
    THROW_EXCEPTION(SessionException("invalid block"));
}

void Session::onExpiredBlock(const PendingBlock_ptr& pendingBlock, bool localExpiration)
{
    ASSERT(m_data.requestingBlock);
    m_data.requestingBlock = false;

    LOG_CLASS(warning) << "Expired block " << pendingBlock->toString() << " " << (localExpiration ? "(local)" : "");
    if (localExpiration) {
        requestBlockHeader();
    } else {
        THROW_EXCEPTION(SessionException("expired block"));
    }
}

void Session::sendFirstPendingTransactions()
{
    ASSERT(!m_data.sharedPendingTransactions);
    LOG_CLASS(trace) << "sending first pending transactions";

    m_data.sharedPendingTransactions = true;
    size_t pendingTransactionsLimit = NewTransactionsPacket::MAX_TRANSACTION_IDS * 4;
    auto pendingTransactions = m_blockchain->getPendingTransactions().getExecutedTransactions(pendingTransactionsLimit);
    std::set<TransactionId> transactionIds;
    for (auto& transaction : pendingTransactions) {
        if (!m_sharedTransactionIds.addTransactionId(transaction->getId()))
            continue;
        transactionIds.insert(transaction->getId());
        if (transactionIds.size() == NewTransactionsPacket::MAX_TRANSACTION_IDS) {
            sendNewTransactionIds(transactionIds);
            transactionIds.clear();
        }
    }

    if (!transactionIds.empty())
        sendNewTransactionIds(transactionIds);
}

void Session::sendNewTransactionIds(const std::set<TransactionId>& transactionIds)
{
    ASSERT(!transactionIds.empty() && transactionIds.size() <= NewTransactionsPacket::MAX_TRANSACTION_IDS);
    LOG_CLASS(trace) << "sending " << transactionIds.size() << " new transaction ids";

    auto newTransactionsPacket = NewTransactionsPacket::create(transactionIds);
    m_connection->send(newTransactionsPacket);
}

void Session::requestNewTransactions()
{
    ASSERT(!m_data.requestingTransactions);
    ASSERT(!m_data.recivedNewTransactionIds.empty());
    LOG_CLASS(debug) << "Requesting transactions";

    if (m_data.lastBlockHeader->getDepth() > m_latestBlockHeader->getDepth()) {
        LOG_CLASS(debug) << "Waiting for new blocks first";
        return;
    }

    size_t transactionsSize = 0;
    std::set<TransactionId> transactionIds;
    for (auto it = m_data.recivedNewTransactionIds.begin(); it != m_data.recivedNewTransactionIds.end(); ) {
        if (transactionsSize + it->getSize() > kTransactionMaxSize)
            break;
        if (!m_blockchain->getTransaction(*it).first) {
            transactionIds.insert(*it);
            transactionsSize += it->getSize();
        }
        it = m_data.recivedNewTransactionIds.erase(it);
    }

    if (transactionIds.empty())
        return;

    LOG_CLASS(debug) << "Requesting (" << transactionIds.size() << ") transactions with size " << transactionsSize;
    m_data.requestingTransactions = true;
    auto packet = GetNewTransactionsPacket::create(transactionIds);
    m_connection->send(packet);
}

void Session::requestBlockHeader()
{
    ASSERT(!m_data.requestingBlock);

    LOG_CLASS(debug) << "Requesting block header (has " << m_latestBlockHeader->getDepth() << " wants " <<
        m_data.lastBlockHeader->getDepth() << ")";

    if (m_latestBlockHeader->getDepth() >= m_data.lastBlockHeader->getDepth() ||
        m_blockchain->getBlockTree().hasBlock(m_data.lastBlockHeader->getHash())) {
        LOG_CLASS(debug) << "There's no newer block header";
        return;
    }

    size_t maxBlocks = GetBlockHeaderPacket::MAX_BLOCKS;
    auto blockIds = m_blockchain->getBlockTree().getBlockIdsAndHashes(maxBlocks,
                                                                      m_data.lastBlockHeader->getDepth() - 1);
    auto getBlockHeaderPacket = GetBlockHeaderPacket::create(blockIds);
    m_data.requestingBlock = true;
    m_connection->send(getBlockHeaderPacket);
}

void Session::requestBlock(const PendingBlock_ptr& pendingBlock)
{
    ASSERT(!m_data.requestingBlock);

    LOG_CLASS(debug) << "Requesting block (" << pendingBlock->toString() << ")";

    auto pendingBlockStatus = pendingBlock->getStatus();
    if (pendingBlockStatus != PendingBlock::Status::MISSING_BODY &&
        pendingBlockStatus != PendingBlock::Status::MISSING_TRANSACTION_IDS &&
        pendingBlockStatus != PendingBlock::Status::MISSING_TRANSACTIONS) {
        LOG_CLASS(debug) << "Requested invalid pending block: " << pendingBlockStatus;

        if (m_blockchain->getBlockTree().isInLastLevel(pendingBlock->getHeaderHash())) {
            LOG_CLASS(debug) << "Waiting for new block";
            m_data.waitingForNewBlock = true;
            m_data.lastRecivedBlockHash = pendingBlock->getHeaderHash();
        } else {
            requestBlockHeader();
        }
        return;
    }

    auto getPendingBlockPacket = GetBlockPacket::create(pendingBlockStatus, pendingBlock);
    m_data.requestingBlock = true;
    m_connection->send(getPendingBlockPacket);
}

json Session::getDebugInfo() const
{
    json j;
    j["local_block_header"] = m_latestBlockHeader ? m_latestBlockHeader->getId() : 0;
    j["remote_block_header"] = m_data.lastBlockHeader ? m_data.lastBlockHeader->getId() : 0;
    j["requesting_block"] = m_data.requestingBlock;
    j["requesting_transactions"] = m_data.requestingTransactions;
    j["waiting_for_new_block"] = m_data.waitingForNewBlock;
    j["shared_pending_transactions"] = m_data.sharedPendingTransactions;
    j["last_recived_block_hash"] = m_data.lastRecivedBlockHash;
    return j;
}

}
