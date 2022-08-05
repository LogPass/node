#include "pch.h"
#include "block.h"

namespace logpass {

Block::Block()
{
    m_header = std::make_shared<BlockHeader>();
    m_body = std::make_shared<BlockBody>();
}

Block::Block(size_t id)
{
    m_header = std::make_shared<BlockHeader>((uint32_t)id);
    m_body = std::make_shared<BlockBody>();
}

Block::Block(const BlockHeader_cptr& header, const BlockBody_cptr& body,
             const std::vector<BlockTransactionIds_cptr>& transactionIds,
             const std::map<TransactionId, Transaction_cptr>& transactions) :
    m_header(header), m_body(body), m_transactionIds(transactionIds), m_transactions(transactions)
{
    ASSERT(m_header && m_body);
    ASSERT(getTransactions() == m_transactions.size());
    ASSERT(((size_t)m_body->getTransactions() + BlockTransactionIds::CHUNK_SIZE - 1) /
        BlockTransactionIds::CHUNK_SIZE == transactionIds.size());
}

Block_cptr Block::create(uint32_t id, uint32_t depth, const MinersQueue& nextMiners,
                         const std::vector<Transaction_cptr>& transactions, const Hash& prevBlockHash,
                         const PrivateKey& privateKey)
{
    ASSERT(nextMiners.size() > 0 && nextMiners.size() <= kMinersQueueSize);
    ASSERT(transactions.size() <= kBlockMaxTransactions);

    // create hashes
    size_t standardTransactions = 0;
    size_t standardTransactionsSize = 0;
    std::map<TransactionId, Transaction_cptr> transactionsMap;
    std::vector<BlockTransactionIds_cptr> blockStandardTransactionIds;
    std::vector<TransactionId> standardTransactionIds;
    for (auto& transaction : transactions) {
        ASSERT(transaction->getSize() > 0 && transaction->getSize() <= kTransactionMaxSize);
        transactionsMap.emplace(transaction->getId(), transaction);
        standardTransactions += 1;
        standardTransactionsSize += transaction->getSize();
        standardTransactionIds.push_back(transaction->getId());
        if (standardTransactionIds.size() == BlockTransactionIds::CHUNK_SIZE) {
            blockStandardTransactionIds.push_back(std::make_shared<BlockTransactionIds>(standardTransactionIds));
            standardTransactionIds.clear();
        }
    }
    if (standardTransactionIds.size() > 0) {
        blockStandardTransactionIds.push_back(std::make_shared<BlockTransactionIds>(standardTransactionIds));
        standardTransactionIds.clear();
    }
    ASSERT(transactionsMap.size() == transactions.size()); // check if hashes are unique
    ASSERT(standardTransactions == transactions.size());
    ASSERT(standardTransactionsSize <= kBlockMaxTransactionsSize);

    // create body
    std::vector<Hash> blockBodyHashes;
    for (auto& it : blockStandardTransactionIds) {
        blockBodyHashes.push_back(it->getHash());
    }

    auto blockBody = std::make_shared<BlockBody>(standardTransactions, standardTransactionsSize, blockBodyHashes);

    // create header
    auto blockHeader = std::make_shared<BlockHeader>(id, depth, prevBlockHash, blockBody->getHash(),
                                                     nextMiners, privateKey);

    // create block
    std::vector<BlockTransactionIds_cptr> blockTransactionIds;
    blockTransactionIds.insert(blockTransactionIds.end(), blockStandardTransactionIds.begin(),
                               blockStandardTransactionIds.end());
    auto block = std::make_shared<Block>(blockHeader, blockBody, blockTransactionIds, transactionsMap);

    return block;
}

void Block::serialize(Serializer& mainSerializer)
{
    if (mainSerializer.writer()) {
        ASSERT(m_header->getId() != 0);
        Serializer s;
        s(m_header);
        s(m_body);
        for (Transaction_cptr transaction : *this) {
            s(transaction);
        }
        mainSerializer.putCompressedData(s);
    } else {
        ASSERT(m_header->getId() == 0);
        Serializer s = Serializer::fromCompressedData(mainSerializer);
        m_header = nullptr;
        m_body = nullptr;
        s(m_header);
        s(m_body);

        // standard transactions
        std::vector<TransactionId> transactionIds;
        for (uint16_t i = 0; i < m_body->getTransactions(); ++i) {
            auto transaction = Transaction::load(s);
            m_transactions.emplace(transaction->getId(), transaction);
            transactionIds.push_back(transaction->getId());
            if (transactionIds.size() == BlockTransactionIds::CHUNK_SIZE) {
                m_transactionIds.push_back(std::make_shared<BlockTransactionIds>(transactionIds));
                transactionIds.clear();
            }
        }
        if (transactionIds.size() > 0) {
            m_transactionIds.push_back(std::make_shared<BlockTransactionIds>(transactionIds));
            transactionIds.clear();
        }

        if (m_transactions.size() != getTransactions()) {
            THROW_SERIALIZER_EXCEPTION("Transactions are not unique");
        }
    }
}

bool Block::validate(const PublicKey& minerKey, const Hash& prevBlockHeaderHash) const
{
    ASSERT(m_header && m_body);

    // validate prev block header hash
    if (m_header->getPrevHeaderHash() != prevBlockHeaderHash) {
        return false;
    }

    if (m_header->getMiner() != MinerId(minerKey)) {
        return false;
    }

    // validate header signature
    if (!m_header->validate(minerKey)) {
        return false;
    }

    // validate body hash
    if (m_header->getBodyHash() != m_body->getHash()) {
        return false;
    }

    if (m_header->getMiner() != minerKey) {
        return false;
    }

    // validate block body transactions and hashes size
    if (getTransactions() != m_transactions.size()) {
        return false;
    }
    if (m_body->getHashes().size() != m_transactionIds.size()) {
        return false;
    }
    if (((size_t)m_body->getTransactions() + BlockTransactionIds::CHUNK_SIZE - 1) / BlockTransactionIds::CHUNK_SIZE
        != m_transactionIds.size()) {
        return false;
    }

    // validate block body hashes and transaction types
    auto& bodyStandardHashes = m_body->getHashes();
    for (size_t i = 0; i < m_transactionIds.size(); ++i) {
        if (bodyStandardHashes[i] != m_transactionIds[i]->getHash()) {
            return false;
        }
    }

    // check hashes and transactions
    size_t transactionsSize = 0;
    std::set<TransactionId> uniqueIds;
    for (auto& transactionIds : m_transactionIds) {
        for (const TransactionId& transactionId : *transactionIds) {
            if (!uniqueIds.insert(transactionId).second) {
                return false; // duplicated hash
            }
            auto transactionIt = m_transactions.find(transactionId);
            if (transactionIt == m_transactions.end()) {
                return false; // missing transaction
            }
            if (transactionIt->second->getId() != transactionId) {
                return false;
            }
            if (transactionIt->second->getSize() != transactionId.getSize()) {
                return false;
            }
            if (transactionId.getSize() == 0 || transactionId.getSize() > kTransactionMaxSize) {
                return false;
            }
            transactionsSize += transactionId.getSize();
        }
    }

    if (uniqueIds.size() != m_transactions.size()) {
        return false;
    }

    if (m_body->getTransactionsSize() != transactionsSize) {
        return false;
    }

    if (transactionsSize > kBlockMaxTransactionsSize) {
        return false;
    }

    return true;
}

TransactionId Block::getTransactionId(size_t index) const
{
    ASSERT(index < getTransactions());
    if (index >= getTransactions()) {
        return TransactionId();
    }
    if (index < m_body->getTransactions()) {
        return m_transactionIds[index / BlockTransactionIds::CHUNK_SIZE]->
            at(index % BlockTransactionIds::CHUNK_SIZE);
    }
    size_t chunksToSkip = (m_body->getTransactions() + BlockTransactionIds::CHUNK_SIZE - 1) /
        BlockTransactionIds::CHUNK_SIZE;
    size_t correctIndex = index - m_body->getTransactions();
    return m_transactionIds[chunksToSkip + correctIndex / BlockTransactionIds::CHUNK_SIZE]->
        at(correctIndex % BlockTransactionIds::CHUNK_SIZE);
}

Transaction_cptr Block::getTransaction(const TransactionId& transactionId) const
{
    auto it = m_transactions.find(transactionId);
    if (it == m_transactions.end()) {
        return nullptr;
    }
    return it->second;
}

}
