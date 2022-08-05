#include "pch.h"
#include "get_block.h"

#include <blockchain/blockchain.h>
#include <blockchain/block/block.h>
#include <blockchain/transactions/transaction.h>
#include <communication/connection/connection.h>
#include <communication/session.h>

namespace logpass {

Packet_ptr GetBlockPacket::create(PendingBlock::Status status, const PendingBlock_ptr& pendingBlock)
{
    ASSERT(status != PendingBlock::Status::COMPLETE);

    auto packet = std::make_shared<GetBlockPacket>();
    packet->m_pendingBlock = pendingBlock;
    packet->m_blockId = pendingBlock->getId();
    packet->m_headerHash = pendingBlock->getHeaderHash();
    packet->m_status = status;

    if (status == PendingBlock::Status::MISSING_BODY) {
        packet->m_bodyHash = pendingBlock->getBlockBodyHash();
    } else if (status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
        packet->m_transactionIdsHashes = pendingBlock->getMissingTransactionIdsHashes(MAX_TRANSACTION_IDS);
    } else if (status == PendingBlock::Status::MISSING_TRANSACTIONS) {
        packet->m_transactionIds = pendingBlock->getMissingTransactionIds(MAX_TRANSACTIONS,
                                                                          MAX_TRANSACTIONS_SIZE);
    }

    return packet;
}

void GetBlockPacket::serializeRequestBody(Serializer& s)
{
    s(m_blockId);
    s(m_headerHash);
    if (s.reader()) {
        m_status = (PendingBlock::Status)s.get<uint8_t>();
    } else {
        s.put<uint8_t>((uint8_t)m_status);
    }
    s(m_expired);
    if (m_status == PendingBlock::Status::MISSING_BODY) {
        s(m_bodyHash);
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
        s(m_transactionIdsHashes);
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTIONS) {
        s(m_transactionIds);
    } else {
        THROW_SERIALIZER_EXCEPTION("Invalid pending block missing part");
    }
}

void GetBlockPacket::serializeResponseBody(Serializer& s)
{
    if (m_status == PendingBlock::Status::MISSING_BODY) {
        s.serializeOptional(m_blockBody);
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
        s(m_blockTransactionIds);
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTIONS) {
        s(m_transactions);
    } else {
        THROW_SERIALIZER_EXCEPTION("Invalid pending block missing part");
    }
}

bool GetBlockPacket::validateRequest()
{
    if (m_status == PendingBlock::Status::MISSING_BODY) {
        return m_bodyHash.isValid();
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
        std::set<uint32_t> uniqueIndexes;
        std::set<Hash> uniqueHashes;
        for (auto& [index, hash] : m_transactionIdsHashes) {
            uniqueIndexes.insert(index);
            uniqueHashes.insert(hash);
        }
        if (uniqueIndexes.size() != m_transactionIdsHashes.size()) {
            return false;
        }
        if (uniqueHashes.size() != m_transactionIdsHashes.size()) {
            return false;
        }
        return m_transactionIdsHashes.size() <= MAX_TRANSACTION_IDS;
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTIONS) {
        if (m_transactionIds.size() > MAX_TRANSACTIONS) {
            return false;
        }
        uint32_t size = 0;
        for (auto& transactionId : m_transactionIds) {
            size += transactionId.getSize();
        }
        if (size > MAX_TRANSACTIONS_SIZE) {
            return false;
        }
        return true;
    }
    return true;
}

bool GetBlockPacket::validateResponse()
{
    if (m_expired) {
        if (m_blockBody || !m_blockTransactionIds.empty() || !m_transactions.empty()) {
            return false;
        }
        return true;
    }

    if (m_status == PendingBlock::Status::MISSING_BODY) {
        if (!m_blockBody) {
            return false;
        }
        if (m_blockBody->getHash() != m_bodyHash) {
            return false;
        }
        return true;
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
        std::set<Hash> uniqueHashes;
        for (auto& [index, hash] : m_transactionIdsHashes) {
            uniqueHashes.insert(hash);
        }
        for (auto& blockTransactionIds : m_blockTransactionIds) {
            auto it = uniqueHashes.find(blockTransactionIds->getHash());
            if (it == uniqueHashes.end()) {
                return false;
            }
            uniqueHashes.erase(it);
        }
        return true;
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTIONS) {
        if (m_transactions.size() != m_transactionIds.size()) {
            return false;
        }
        std::set<TransactionId> uniqueTransactionIds;
        for (auto& transaction : m_transactions) {
            if (m_transactionIds.count(transaction->getId()) == 0) {
                return false;
            }
            uniqueTransactionIds.insert(transaction->getId());
        }
        if (uniqueTransactionIds.size() != m_transactions.size()) {
            return false;
        }
        return true;
    }
    return false;
}

void GetBlockPacket::executeRequest(const PacketExecutionParams& params)
{
    // first try to find block in active branch of BlockTree
    auto activeBranch = params.blockTree->getActiveBranch();
    auto it = std::find_if(activeBranch.begin(), activeBranch.end(), [&](auto& node) {
        return node.getHeaderHash() == m_headerHash;
    });
    if (it != activeBranch.end()) {
        Block_cptr& block = it->block;
        if (m_status == PendingBlock::Status::MISSING_BODY) {
            m_blockBody = block->getBlockBody();
        } else if (m_status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
            auto blockTransactionIds = block->getBlockTransactionIds();
            for (auto& [index, hash] : m_transactionIdsHashes) {
                if (index >= blockTransactionIds.size()) {
                    THROW_PACKET_EXCEPTION("Requested invalid transactions id");
                }
                m_blockTransactionIds.push_back(blockTransactionIds[index]);
            }
        } else if (m_status == PendingBlock::Status::MISSING_TRANSACTIONS) {
            for (auto& transactionId : m_transactionIds) {
                Transaction_cptr transaction = block->getTransaction(transactionId);
                if (!transaction) {
                    THROW_PACKET_EXCEPTION("Requested invalid transaction");
                }
                m_transactions.push_back(transaction);
            }
        }
        return;
    }


    // get required data from database
    if (m_status == PendingBlock::Status::MISSING_BODY) {
        BlockBody_cptr blockBody = params.database->blocks.getBlockBody(m_blockId);
        if (!blockBody || blockBody->getHash() != m_bodyHash) {
            m_expired = true;
            return;
        }
        m_blockBody = blockBody;
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
        for (auto& [index, hash] : m_transactionIdsHashes) {
            auto blockTransactionIds = params.database->blocks.getBlockTransactionIds(m_blockId, index);
            if (!blockTransactionIds || blockTransactionIds->getHash() != hash) {
                m_expired = true;
                m_blockTransactionIds.clear();
                return;
            }
            m_blockTransactionIds.push_back(blockTransactionIds);
        }
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTIONS) {
        for (auto& transactionId : m_transactionIds) {
            auto [transaction, blockId] = params.blockchain->getTransaction(transactionId);
            if (!transaction) {
                m_expired = true;
                m_transactions.clear();
                return;
            }
            m_transactions.push_back(transaction);
        }
    }
}

void GetBlockPacket::executeResponse(const PacketExecutionParams& params)
{
    // check if block expired
    if (m_expired) {
        return params.session->onExpiredBlock(m_pendingBlock, false);
    }

    // check if block is still valid
    auto blockStatus = m_pendingBlock->getStatus();
    if (blockStatus == PendingBlock::Status::INVALID) {
        return params.session->onInvalidBlock(m_pendingBlock);
    } else if (blockStatus == PendingBlock::Status::EXPIRED) {
        return params.session->onExpiredBlock(m_pendingBlock, true);
    } else if (blockStatus == PendingBlock::Status::COMPLETE || blockStatus == PendingBlock::Status::FINISHED) {
        return params.session->onCompletedBlock(m_pendingBlock);
    }

    // update block
    bool validData = false;
    if (m_status == PendingBlock::Status::MISSING_BODY) {
        auto status = m_pendingBlock->addBlockBody(m_blockBody);
        if (status == PendingBlock::AddResult::CORRECT || status == PendingBlock::AddResult::DUPLICATED)
            validData = true;
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTION_IDS) {
        auto status = m_pendingBlock->addBlockTransactionIds(m_blockTransactionIds);
        if (status == PendingBlock::AddResult::CORRECT || status == PendingBlock::AddResult::DUPLICATED) {
            validData = true;
        } else {
            validData = false;
        }
    } else if (m_status == PendingBlock::Status::MISSING_TRANSACTIONS) {
        params.blockchain->addTransactions(m_transactions, params.session->getMiner());
        validData = true;
    }

    // update block status
    blockStatus = m_pendingBlock->getStatus();
    // check if data and pending block are valid
    if (!validData || blockStatus == PendingBlock::Status::INVALID) {
        return params.session->onInvalidBlock(m_pendingBlock);
    } else if (blockStatus == PendingBlock::Status::EXPIRED) {
        return params.session->onExpiredBlock(m_pendingBlock, true);
    } else if (blockStatus == PendingBlock::Status::COMPLETE || blockStatus == PendingBlock::Status::FINISHED) {
        return params.session->onCompletedBlock(m_pendingBlock);
    }

    // block is not ready, request next part
    params.connection->send(GetBlockPacket::create(blockStatus, m_pendingBlock));
}

}
