#pragma once

#include "api_options.h"
#include "api_server.h"
#include "blockchain/blockchain.h"
#include "communication/communication.h"

namespace logpass {

class Api : public Thread {
protected:
    friend class SharedThread<Api>;

    Api(const ApiOptions& options, const std::shared_ptr<Blockchain>& blockchain,
        const std::shared_ptr<const Database>& database,
        const std::shared_ptr<const Communication>& communication = nullptr);
    void start() override;
    void stop() override;

public:
    ApiOptions getOptions() const
    {
        return m_options;
    }

protected:
    ApiServer configureServer();

    void onWebsocketMessage(ApiServer::WebSocket* ws, const json& data);

    void onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch);
    void onNewTransactions(const std::vector<Transaction_cptr>& transactions);

    bool onlyConfirmed(ApiServer::HttpRequest* req) const;
    uint32_t toU32(const std::string_view& str) const;

    json status() const;
    json health() const;
    json readiness() const;
    json liveness() const;

    json getBlock(uint32_t blockId) const;
    json getBlockBody(uint32_t blockId) const;
    json getBlockHeader(uint32_t blockId) const;
    json getNextBlockHeader(uint32_t blockId) const;
    json getBlockTransactionIds(uint32_t blockId, uint32_t page) const;
    json getFirstBlock() const;
    json getLatestBlocks() const;

    json getUsersUpdatedInBlockCount(uint32_t blockId) const;
    json getUsersUpdatedInBlock(uint32_t blockId, uint32_t page) const;

    json getMiner(const MinerId& minerId, bool confirmed = true) const;
    json getMinersQueue() const;
    json getTopMiners() const;
    json getTrustedMiners() const;
    json getTransaction(const TransactionId& transactionId, bool confirmed = true) const;
    json getUser(const UserId& userId, bool confirmed = true) const;
    json getUserHistory(const UserId& userId, uint32_t page, bool confirmed = true) const;
    json getUserSponsors(const UserId& userId, uint32_t page, bool confirmed = true) const;

    json getPrefix(const std::string& prefixId, bool confirmed = true) const;
    json getStorageEntry(const std::string& prefixId, const std::string& key, bool confirmed = true) const;
    json getTransactionsForPrefix(const std::string& prefixId, uint32_t page) const;

    void postTransaction(Serializer& serializer, SafeCallback<json>&& callback) const;

    void getDebugInfo(SafeCallback<json>&& callback) const;

    const DatabaseFacades* db(bool confirmed = true) const
    {
        if (!confirmed) {
            return &(m_database->unconfirmed());
        }
        return &(m_database->confirmed());
    }

private:
    const ApiOptions m_options;
    const std::shared_ptr<Blockchain> m_blockchain;
    const std::shared_ptr<const Database> m_database;
    const std::shared_ptr<const Communication> m_communication;
    mutable Logger m_logger;

    std::unique_ptr<EventsListener> m_eventsListener;
    ApiServer* m_server = nullptr;
    uWS::Loop* m_loop = nullptr;

    std::set<ApiServer::WebSocket*> m_websockets;

    std::atomic<bool> m_stopped = false;
};

}
