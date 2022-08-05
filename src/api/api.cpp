#include "pch.h"

#include "api.h"
#include <blockchain/transactions/storage/add_entry.h>

namespace logpass {

Api::Api(const ApiOptions& options, const std::shared_ptr<Blockchain>& blockchain,
         const std::shared_ptr<const Database>& database, const std::shared_ptr<const Communication>& communication) :
    m_options(options), m_blockchain(blockchain), m_database(database), m_communication(communication)
{
    ASSERT(m_options.port != 0);

    m_logger.add_attribute("Class", boost::log::attributes::constant<std::string>("Api"));
    m_logger.add_attribute("ID", boost::log::attributes::constant<std::string>(
        m_options.host + ":"s + std::to_string(m_options.port)));
}

void Api::start()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    std::condition_variable_any cv;

    m_thread = std::thread([this, &cv] {
        SET_THREAD_NAME("api");
        m_mutex.lock();
        m_loop = uWS::Loop::get();

        ApiServer server = configureServer().listen(m_options.host, m_options.port, [this](bool success) {
            if (success) {
                LOG_CLASS(info) << "API started on " << m_options.host << ":" << m_options.port;
            } else {
                LOG_CLASS(error) << "API can not start on " << m_options.host << ":" << m_options.port;
            }
        });
        m_server = &server;

        m_eventsListener = m_blockchain->registerEventsListener(EventsListenerCallbacks{
            .onBlocks = [this](auto blocks, bool didChangeBranch) {
                m_loop->defer(std::bind(&Api::onBlocks, this, blocks, didChangeBranch));
            },
            .onNewTransactions = [this](auto transactions) {
                m_loop->defer(std::bind(&Api::onNewTransactions, this, transactions));
            }
        });

        cv.notify_all();
        m_mutex.unlock();

        server.run();

        std::lock_guard lock(m_mutex);
        m_server = nullptr;
        m_loop = nullptr;
    });

    cv.wait(lock, [this] {return m_loop != nullptr; });
}

void Api::stop()
{
    m_mutex.lock();
    m_stopped = true;
    m_eventsListener = nullptr;
    m_websockets.clear();
    if (m_server) {
        m_server->close();
    }
    m_mutex.unlock();
    Thread::stop();
}

ApiServer Api::configureServer()
{
    ApiServer::WebSocketBehavior webSocketBehavior = {
        .maxPayloadLength = 1024 + kTransactionMaxSize * 2,
        .maxBackpressure = 2 * 1024 * 1024
    };
    webSocketBehavior.open = [this](ApiServer::WebSocket* ws) {
        m_websockets.insert(ws);
    };
    webSocketBehavior.message = [this](ApiServer::WebSocket* ws, std::string_view message, uWS::OpCode opCode) {
        if (message.size() > kTransactionMaxSize * 2) {
            return ws->end(0, "Too long data");
        }
        auto data = json::parse(message.begin(), message.end(), nullptr, false);
        if (data == json::value_t::discarded) {
            return ws->end(0, "Invalid data");
        }
        onWebsocketMessage(ws, data);
    };
    webSocketBehavior.close = [this](ApiServer::WebSocket* ws, int code, std::string_view message) {
        m_websockets.erase(ws);
    };

    return ApiServer().ws("/", std::move(webSocketBehavior)).options("/*", [this](auto* req) {
        return json();
    }).get("/", [this](auto* req) {
        return status();
    }).get("/status", [this](auto* req) {
        return status();
    }).get("/health", [this](auto* req) {
        return health();
    }).get("/health/readiness", [this](auto* req) {
        return readiness();
    }).get("/health/liveness", [this](auto* req) {
        return liveness();
    }).get("/blocks/:id", [this](auto* req) {
        return getBlock(toU32(req->getParameter(0)));
    }).get("/blocks/:id/body", [this](auto* req) {
        return getBlockBody(toU32(req->getParameter(0)));
    }).get("/blocks/:id/header", [this](auto* req) {
        return getBlockHeader(toU32(req->getParameter(0)));
    }).get("/blocks/:id/header/next", [this](auto* req) {
        return getNextBlockHeader(toU32(req->getParameter(0)));
    }).get("/blocks/:id/transactions/:chunk", [this](auto* req) {
        return getBlockTransactionIds(toU32(req->getParameter(0)), toU32(req->getParameter(1)));
    }).get("/blocks/:id/users", [this](auto* req) {
        return getUsersUpdatedInBlockCount(toU32(req->getParameter(0)));
    }).get("/blocks/:id/users/:page", [this](auto* req) {
        return getUsersUpdatedInBlock(toU32(req->getParameter(0)), toU32(req->getParameter(1)));
    }).get("/blocks/first", [this](auto* req) {
        return getFirstBlock();
    }).get("/blocks/latest", [this](auto* req) {
        return getLatestBlocks();
    }).get("/miners/:id", [this](auto* req) {
        return getMiner(MinerId(req->getParameter(0)), onlyConfirmed(req));
    }).get("/miners/queue", [this](auto* req) {
        return getMinersQueue();
    }).get("/miners/top", [this](auto* req) {
        return getTopMiners();
    }).get("/miners/trusted", [this](auto* req) {
        return getTrustedMiners();
    }).get("/status", [this](auto* req) {
        return status();
    }).get("/storage/prefixes/:id", [this](auto* req) {
        return getPrefix(std::string(req->getParameter(0)), onlyConfirmed(req));
    }).get("/storage/prefixes/:id/transactions/:page", [this](auto* req) {
        return getTransactionsForPrefix(std::string(req->getParameter(0)), toU32(req->getParameter(1)));
    }).get("/storage/entries/:id", [this](auto* req) {
        return getStorageEntry("", std::string(req->getParameter(0)), onlyConfirmed(req));
    }).get("/storage/entries/:prefix/:id", [this](auto* req) {
        return getStorageEntry(std::string(req->getParameter(0)), std::string(req->getParameter(1)),
                               onlyConfirmed(req));
    }).get("/transactions/:id", [this](auto* req) {
        return getTransaction(TransactionId(req->getParameter(0)), onlyConfirmed(req));
    }).get("/users/:id", [this](auto* req) {
        return getUser(UserId(req->getParameter(0)), onlyConfirmed(req));
    }).get("/users/:id/history", [this](auto* req) {
        return getUserHistory(UserId(req->getParameter(0)), 0);
    }).get("/users/:id/history/:page", [this](auto* req) {
        return getUserHistory(UserId(req->getParameter(0)), toU32(req->getParameter(1)), onlyConfirmed(req));
    }).get("/users/:id/sponsors", [this](auto* req) {
        return getUserSponsors(UserId(req->getParameter(0)), 0);
    }).get("/users/:id/sponsors/:page", [this](auto* req) {
        return getUserSponsors(UserId(req->getParameter(0)), toU32(req->getParameter(1)), onlyConfirmed(req));
    }).get("/debug", [this](auto* req, SafeCallback<json>&& handler) {
        return getDebugInfo(std::move(handler));
    }).post("/transactions", [this](auto* req, auto serializer, auto handler) {
        postTransaction(*serializer, std::move(handler));
    }).any("/*", [this](auto* req) {
        return 404;
    });
}

void Api::onWebsocketMessage(ApiServer::WebSocket* ws, const json& data)
{
    json response = {
        {"type", "response"}
    };
    try {
        if (data.contains("_id") && (data["_id"].is_string() || data["_id"].is_number())) {
            response["_id"] = data["_id"];
        }
    } catch (const json::exception& exception) {
        response["error"] = "JSON _id parse exception: "s + exception.what();
        ws->send(response.dump(), uWS::TEXT);
        return;
    }

    try {
        bool unconfirmed = data.contains("unconfirmed") && (data["unconfirmed"].get<bool>() == true);
        std::string type = data["type"];
        if (type == "subscribe") {
            ws->subscribe(data["topic"].get<std::string>());
        } else if (type == "unsubscribe") {
            ws->unsubscribe(data["topic"].get<std::string>());
        } else if (type == "status") {
            response["data"] = status();
        } else if (type == "user") {
            response["data"] = getUser(data["user"], unconfirmed);
        } else if (type == "miner") {
            response["data"] = getMiner(data["miner"], unconfirmed);
        } else if (type == "block") {
            response["data"] = getBlock(data["block"].get<uint32_t>());
        } else if (type == "prefix") {
            response["data"] = getPrefix(data["prefix"].get<std::string>(), unconfirmed);
        } else if (type == "storage") {
            std::string prefix = data["prefix"].get<std::string>();
            std::string key = data["key"].get<std::string>();
            response["data"] = getStorageEntry(prefix, key, unconfirmed);
        } else if (type == "transaction") {
            auto transaction = data["transaction"].get<std::string>();
            if (transaction.size() <= 96) {
                response["data"] = getTransaction(TransactionId(transaction), unconfirmed);
            } else {
                auto serializer = Serializer::fromBase64(transaction);
                auto weakSelf = weak_from_this();
                postTransaction(serializer, (SafeCallback<json>)
                                [this, ws, response, weakSelf](const std::shared_ptr<json>& data) mutable {
                    auto self = weakSelf.lock();
                    if (!self) {
                        return;
                    }
                    std::unique_lock lock(m_mutex);
                    if (m_stopped) {
                        return;
                    }
                    m_loop->defer([this, ws, response, data]() mutable {
                        if (m_websockets.find(ws) == m_websockets.end()) {
                            return;
                        }
                        response["data"] = *data;
                        ws->send(response.dump(), uWS::TEXT);
                    });
                });
                return;
            }
        } else {
            response["error"] = "Invalid request type";
        }
    } catch (const json::exception& exception) {
        response["error"] = "JSON exception: "s + exception.what();
    } catch (const SerializerException& exception) {
        response["error"] = "Serializer exception: "s + exception.what();
    }
    ws->send(response.dump(), uWS::TEXT);
}

void Api::onBlocks(const std::vector<Block_cptr>& blocks, bool didChangeBranch)
{
    json blocksJSON;
    for (auto& block : blocks) {
        blocksJSON.push_back(json{
            {"id", block->getId()},
            {"depth", block->getDepth()},
            {"header", block->getBlockHeader()},
            {"body", block->getBlockBody()}
        });
    }

    m_server->publish("new_blocks", {
        {"type", "subscription"},
        {"topic", "new_blocks"},
        {"data", {
            {"blocks", blocksJSON},
            {"did_change_branch", didChangeBranch}
        }}
    });
}

void Api::onNewTransactions(const std::vector<Transaction_cptr>& transactions)
{
    std::vector<TransactionId> transactionIds;
    for (auto& transaction : transactions) {
        transactionIds.push_back(transaction->getId());
    }

    m_server->publish("new_transactions", {
        {"type", "subscription"},
        {"topic", "new_transactions"},
        {"data", {
            {"transaction_ids", transactionIds}
        }}
    });
}

bool Api::onlyConfirmed(ApiServer::HttpRequest* req) const
{
    auto parameter = req->getQuery("unconfirmed");
    return !(parameter == "true"s || parameter == "True"s || parameter == "TRUE"s || parameter == "1"s);
}

uint32_t Api::toU32(const std::string_view& str) const
{
    uint32_t value;
    auto result = std::from_chars(str.data(), str.data() + str.size(), value);
    if (result.ec != std::errc()) {
        return 0;
    }
    return value;
}

json Api::status() const
{
    json j;
    j["version"] = std::to_string(kVersionMajor) + "."s + std::to_string(kVersionMinor);
    j["miner_id"] = m_blockchain->getMinerId();
    j["pricing"] = db()->state.getPricing();
    j["initialization_time"] = m_blockchain->getInitializationTime();
    j["current_time"] = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
    j["latest_block_id"] = m_blockchain->getLatestBlockId();
    j["exptected_block_id"] = m_blockchain->getExpectedBlockId();
    j["is_synchronized"] = !m_blockchain->isDesynchronized();
    j["miners"] = db()->miners.getMinersCount();
    j["tokens"] = db()->users.getTokens();
    j["staked_tokens"] = db()->miners.getStakedTokens();
    j["users"] = db()->users.getUsersCount();
    j["transactions"] = db()->transactions.getTransactionsCount();
    j["transactions_size"] = db()->transactions.getTransactionsSize();
    j["new_transactions"] = db(false)->transactions.getNewTransactionsCount();
    j["new_transactions_size"] = db(false)->transactions.getNewTransactionsSize();
    j["pending_transactions"] = m_blockchain->getPendingTransactions().getPendingTransactionsCount();
    j["pending_transactions_size"] = m_blockchain->getPendingTransactions().getPendingTransactionsSize();
    j["storage"] = {
        {"entries", db()->storage.getEntriesCount()},
        {"prefixes", db()->storage.getPrefixesCount()},
    };
    return j;
}

json Api::health() const
{
    return {
        {"readiness", readiness()},
        {"liveness", liveness()},
    };
}

json Api::readiness() const
{
    auto now = chrono::high_resolution_clock::now();
    db()->users.getRandomUser();
    chrono::duration<float> duration = chrono::high_resolution_clock::now() - now;

    json status = {
        {"is_ready", true},
        {"is_synchronized", m_blockchain->getLatestBlockId() + 32 > m_blockchain->getExpectedBlockId()},
        {"detail", "ready"},
        {"time_taken", duration.count()},
    };
    return {
        {"logpass", status}
    };
}

json Api::liveness() const
{
    return {
        {"status", "ok"}
    };
}

json Api::getBlock(uint32_t blockId) const
{
    auto header = db()->blocks.getBlockHeader(blockId);
    if (!header) {
        return 404;
    }
    auto body = db()->blocks.getBlockBody(blockId);
    if (!body || body->getHash() != header->getBodyHash()) {
        return 404;
    }
    return {
        {"id", header->getId()},
        {"depth", header->getDepth()},
        {"header", header},
        {"body", body},
    };
}

json Api::getBlockHeader(uint32_t blockId) const
{
    auto header = db()->blocks.getBlockHeader(blockId);
    if (!header) {
        return 404;
    }
    return header;
}

json Api::getNextBlockHeader(uint32_t blockId) const
{
    auto header = db()->blocks.getNextBlockHeader(blockId);
    if (!header) {
        return 404;
    }
    return header;
}

json Api::getBlockBody(uint32_t blockId) const
{
    auto body = db()->blocks.getBlockBody(blockId);
    if (!body) {
        return 404;
    }
    return body;
}

json Api::getBlockTransactionIds(uint32_t blockId, uint32_t page) const
{
    auto transactionIds = db()->blocks.getBlockTransactionIds(blockId, page);
    if (!transactionIds) {
        return 404;
    }
    return transactionIds;
}

json Api::getFirstBlock() const
{
    auto firstBlock = db()->blocks.getBlock(1);
    if (!firstBlock) {
        return 404;
    }

    Serializer s;
    s(firstBlock);

    json blocks;
    blocks.push_back(s.base64());

    return blocks;
}

json Api::getLatestBlocks() const
{
    json j;
    for (auto& [blockId, headerAndBody] : db()->blocks.getLatestBlocks()) {
        j.push_back(json{
            {"id", blockId},
            {"depth", headerAndBody.first->getDepth()},
            {"header", headerAndBody.first},
            {"body", headerAndBody.second}
        });
    }
    return j;
}

json Api::getUsersUpdatedInBlockCount(uint32_t blockId) const
{
    return json{
        {"block_id", blockId},
        {"updated_users_count", db()->users.getUpdatedUserIdsCount(blockId)}
    };
}

json Api::getUsersUpdatedInBlock(uint32_t blockId, uint32_t page) const
{
    return db()->users.getUpdatedUserIds(blockId, page);
}

json Api::getTransaction(const TransactionId& transactionId, bool confirmed) const
{
    // it is prefered to get transaction from database because it has information about commit
    auto [transaction, blockId] = m_blockchain->getTransaction(transactionId);

    if (!transaction) {
        return 404;
    }

    if (blockId == 0 && confirmed) {
        return 404; // not confirmed
    }

    json j = transaction;
    j["committed_in"] = blockId;
    if (blockId > 0) {
        uint64_t initializationTime = m_blockchain->getInitializationTime();
        j["committed_at"] = initializationTime + blockId * m_blockchain->getBlockInterval();
    } else {
        j["committed_at"] = 0;
    }

    return j;
}

json Api::getMiner(const MinerId& minerId, bool confirmed) const
{
    auto miner = db(confirmed)->miners.getMiner(minerId);
    if (!miner) {
        return 404;
    }
    return miner;
}

json Api::getMinersQueue() const
{
    return db()->blocks.getMinersQueue();
}

json Api::getTopMiners() const
{
    json j;
    for (auto& miner : db()->miners.getTopMiners()) {
        j.push_back({
            {"id", miner->id},
            {"api", miner->settings.api},
            {"endpoint", miner->settings.endpoint},
            {"name", miner->settings.name},
            {"website", miner->settings.website},
            {"stake", miner->stake}
        });
    }
    return j;
}

json Api::getTrustedMiners() const
{
    auto topMiners = db()->miners.getTopMiners();
    if (topMiners.empty()) {
        return 500;
    }
    size_t firstMinerStake = (*topMiners.begin())->stake;
    size_t minimumStake = firstMinerStake / 1000;
    json trustedMiners = std::map<std::string, std::string>();
    size_t count = 0;
    for (auto& miner : topMiners) {
        if (miner->stake < minimumStake) {
            break;
        }
        if (!miner->settings.endpoint.isValid()) {
            continue;
        }
        trustedMiners.emplace(miner->id.toBase64(), miner->settings.endpoint.toString());
    }
    return trustedMiners;
}

json Api::getUser(const UserId& userId, bool confirmed) const
{
    auto user = db(confirmed)->users.getUser(userId);
    if (!user) {
        return 404;
    }
    return user;
}

json Api::getUserHistory(const UserId& userId, uint32_t page, bool confirmed) const
{
    return db(confirmed)->users.getUserHistory(userId, page);
}

json Api::getUserSponsors(const UserId& userId, uint32_t page, bool confirmed) const
{
    return db(confirmed)->users.getUserSponsors(userId, page);
}

json Api::getPrefix(const std::string& prefixId, bool confirmed) const
{
    auto prefix = db(confirmed)->storage.getPrefix(prefixId);
    if (!prefix) {
        return 404;
    }
    json j = prefix;
    uint64_t initializationTime = m_blockchain->getInitializationTime();
    j["created_at"] = initializationTime + prefix->created * m_blockchain->getBlockInterval();
    if (prefix->lastEntry) {
        j["last_entry_at"] = initializationTime + prefix->lastEntry * m_blockchain->getBlockInterval();
    } else {
        j["last_entry_at"] = 0;
    }
    return j;
}

json Api::getStorageEntry(const std::string& prefixId, const std::string& key, bool confirmed) const
{
    std::string decodedKey;
    try {
        std::vector<uint8_t> decoded;
        try {
            decoded = base64url_unpadded::decode(key);
        } catch (cppcodec::parse_error&) {
            decoded = base64::decode(key);
        }
        decodedKey = std::string(decoded.begin(), decoded.end());
    } catch (cppcodec::parse_error&) {
        return 400;
    }

    auto entry = db(confirmed)->storage.getEntry(prefixId, decodedKey);
    if (!entry) {
        return 404;
    }

    auto [transaction, blockId] = m_blockchain->getTransaction(entry->transactionId);
    if (!transaction || transaction->getType() != StorageAddEntryTransaction::TYPE) {
        return 404;
    }

    if (blockId == 0 && confirmed) {
        return 404; // not confirmed
    }

    auto storageAddEntryTransaction = std::dynamic_pointer_cast<const StorageAddEntryTransaction>(transaction);
    if (!storageAddEntryTransaction) {
        return 500;
    }

    json j = entry;
    j["committed_in"] = blockId;
    if (blockId > 0) {
        uint64_t initializationTime = m_blockchain->getInitializationTime();
        j["committed_at"] = initializationTime + blockId * m_blockchain->getBlockInterval();
    } else {
        j["committed_at"] = 0; // not committed
    }
    j["prefix"] = storageAddEntryTransaction->getPrefix();
    j["key"] = base64url::encode(storageAddEntryTransaction->getKey());
    j["value"] = base64url::encode(storageAddEntryTransaction->getValue());
    return j;
}

json Api::getTransactionsForPrefix(const std::string& prefixId, uint32_t page) const
{
    return db()->storage.getTransasctionsForPrefix(prefixId, page);
}

void Api::postTransaction(Serializer& serializer, SafeCallback<json>&& callback) const
{
    m_blockchain->postTransaction(serializer, (PostTransactionCallback)[callback = std::move(callback)](auto result) {
        return callback(std::move((json)result));
    });
}

void Api::getDebugInfo(SafeCallback<json>&& callback) const
{
    std::shared_ptr<json> j = std::make_shared<json>();
    j->emplace("blockchain", m_blockchain->getDebugInfo());
    j->emplace("database", m_database->getDebugInfo());

    if (m_communication) {
        m_communication->getDebugInfo((SafeCallback<json>)[j, callback = std::move(callback)](const std::shared_ptr<json>& ret) {
            j->emplace("communication", ret ? *ret : json());
            callback(j);
        });
        return;
    }

    callback(j);
}

}
