#pragma once

namespace logpass {

class Blockchain;
class BlockTree;
class Connection;
class Communication;
struct ConfirmedDatabase;
class PendingTransactions;
class Session;
struct SessionData;

struct PacketExecutionParams {
    const ConfirmedDatabase* const database = nullptr;
    Blockchain* const blockchain = nullptr;
    BlockTree* const blockTree = nullptr;
    Connection* const connection = nullptr;
    Session* const session = nullptr;
    SessionData* const sessionData = nullptr;
};

}
