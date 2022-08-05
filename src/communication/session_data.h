#pragma once

#include <blockchain/block/block_header.h>

namespace logpass {

struct SessionData {
    BlockHeader_cptr lastBlockHeader = nullptr;
    chrono::steady_clock::time_point lastBlockHeaderTime;
    bool requestingBlock = false;
    bool requestingTransactions = false;
    bool sharedPendingTransactions = false;
    bool waitingForNewBlock = false;
    Hash lastRecivedBlockHash;
    std::set<TransactionId> newTransactionIds;
    std::set<TransactionId> recivedNewTransactionIds;
};

}
