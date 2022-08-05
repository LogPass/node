#pragma once

#include "api_server.h"

namespace logpass {

class ApiServer : private uWS::App {
public:
    struct UserData {};

    using WebSocket = uWS::WebSocket<false, true, UserData>;
    using HttpResponse = uWS::HttpResponse<false>;
    using HttpRequest = uWS::HttpRequest;
    using WebSocketBehavior = uWS::App::WebSocketBehavior<UserData>;

    struct ApiServerData {
        uWS::Loop* loop = nullptr;
        us_listen_socket_t* socket = nullptr;
        std::set<HttpResponse*> pendingResponses;
        std::set<WebSocket*> websockets;
        bool stopped = false;
        std::mutex mutex;
    };

    ApiServer() : uWS::App::TemplatedApp(), data(std::make_shared<ApiServerData>())
    {}

    ApiServer(uWS::App&& app, std::shared_ptr<ApiServerData>& serverData) : uWS::App(std::move(app)), data(serverData)
    {
        serverData = nullptr;
    }

    ~ApiServer()
    {
        ASSERT(!data || data->stopped);
        ASSERT(!data || data->pendingResponses.empty());
        ASSERT(!data || data->websockets.empty());
    }

    void close()
    {
        std::lock_guard<std::mutex> lock(data->mutex);
        ASSERT(!data->stopped);
        ASSERT(data->loop);
        data->stopped = true;
        data->loop->defer([data = data] {
            ASSERT(data->socket);
            us_listen_socket_close(0, data->socket);
            data->socket = nullptr;
            data->loop->defer([data = data] {
                for (auto& response : data->pendingResponses) {
                    // needs to be deferred because modifies pendingResponses
                    data->loop->defer([response, data = data] {
                        if (!data->pendingResponses.contains(response)) {
                            return;
                        }
                        response->close();
                    });
                }
                for (auto& websocket : data->websockets) {
                    // needs to be deferred because modifies websockets
                    data->loop->defer([websocket, data = data] {
                        if (!data->websockets.contains(websocket)) {
                            return;
                        }
                        websocket->close();
                    });
                }
                data->loop->defer([data = data] {
                    data->loop = nullptr;
                });
            });
        });
    }

    ApiServer ws(const std::string& pattern, ApiServer::WebSocketBehavior&& behavior)
    {
        behavior.open = [data = data, handler = std::move(behavior.open)](WebSocket* ws) mutable {
            data->websockets.insert(ws);
            handler(ws);
        };
        behavior.close = [data = data, handler = std::move(behavior.close)](WebSocket* ws, int code,
                                                                            std::string_view message) mutable {
            data->websockets.erase(ws);
            handler(ws, code, message);
        };
        return { uWS::App::ws(pattern, std::move(behavior)), data };
    }

    ApiServer get(const std::string& pattern, uWS::MoveOnlyFunction<void(HttpResponse*, HttpRequest*)>&& handler)
    {
        return { uWS::App::get(pattern, std::move(handler)), data };
    }

    ApiServer post(const std::string& pattern, std::function<json(HttpRequest*, Serializer_ptr)>&& handler)
    {
        return { uWS::App::post(pattern,[data = data, handler = std::move(handler)](auto* res, auto* req) mutable {
            data->pendingResponses.insert(res);
            res->onAborted([res, data] {
                data->pendingResponses.erase(res);
            });

            Serializer_ptr s = std::make_shared<Serializer>();
            res->onData([s, req, res, data, handler](std::string_view msg, bool last) mutable {
                if (!data->pendingResponses.contains(res)) {
                    return;
                }
                s->readFrom(msg);
                if (s->pos() > kTransactionMaxSize * 2) {
                    data->pendingResponses.erase(res);
                    res->close();
                    return;
                }
                if (!last) {
                    return;
                }
                data->pendingResponses.erase(res);
                s->switchToReader();
                handler(req, s);
            });
        }), data };
    }


    ApiServer post(const std::string& pattern,
                   std::function<void(HttpRequest*, Serializer_ptr, SafeCallback<json>&&)>&& handler)
    {
        return { uWS::App::post(pattern,[data = data, handler = std::move(handler)](auto* res, auto* req) mutable {
            data->pendingResponses.insert(res);
            res->onAborted([res, data] {
                data->pendingResponses.erase(res);
            });

            Serializer_ptr s = std::make_shared<Serializer>();
            res->onData([s, req, res, data, handler](std::string_view msg, bool last) mutable {
                if (!data->pendingResponses.contains(res)) {
                    return;
                }
                s->readFrom(msg);
                if (s->pos() > kTransactionMaxSize * 2) {
                    data->pendingResponses.erase(res);
                    res->close();
                    return;
                }
                if (!last) {
                    return;
                }
                s->switchToReader();
                handler(req, s, (SafeCallback<json>)[res, data](const std::shared_ptr<json>& json) {
                    std::lock_guard<std::mutex> lock(data->mutex);
                    if (data->stopped) {
                        return;
                    }
                    data->loop->defer([res, json, data] {
                        if (!data->pendingResponses.contains(res)) {
                            return;
                        }
                        data->pendingResponses.erase(res);
                        writeResponse(res, *json);
                    });
                });
            });
        }), data };
    }

    ApiServer get(const std::string& pattern,
                  uWS::MoveOnlyFunction<void(HttpRequest*, SafeCallback<json>&&)>&& handler)
    {
        return { uWS::App::get(pattern,[data = data, handler = std::move(handler)](auto* res, auto* req) mutable {
            data->pendingResponses.insert(res);
            res->onAborted([res, data] {
                data->pendingResponses.erase(res);
            });

            handler(req, (SafeCallback<json>)[res, data](const std::shared_ptr<json>& json) {
                std::lock_guard<std::mutex> lock(data->mutex);
                if (data->stopped) {
                    return;
                }
                data->loop->defer([res, json, data] {
                    if (!data->pendingResponses.contains(res)) {
                        return;
                    }
                    data->pendingResponses.erase(res);
                    ApiServer::writeResponse(res, *json);
                });
            });
        }), data };
    }

    ApiServer get(const std::string& pattern, uWS::MoveOnlyFunction<json(HttpRequest*)>&& handler)
    {
        return { uWS::App::get(pattern,[handler = std::move(handler)](auto* res, auto* req) mutable {
            return ApiServer::writeResponse(res, handler(req));
        }), data };
    }

    ApiServer options(const std::string& pattern, uWS::MoveOnlyFunction<json(HttpRequest*)>&& handler)
    {
        return { uWS::App::options(pattern,[handler = std::move(handler)](auto* res, auto* req) mutable {
            return ApiServer::writeResponse(res, handler(req));
        }), data };
    }

    ApiServer any(const std::string& pattern, uWS::MoveOnlyFunction<json(HttpRequest*)>&& handler)
    {
        return { uWS::App::any(pattern,[handler = std::move(handler)](auto* res, auto* req) mutable {
            return ApiServer::writeResponse(res, handler(req));
        }), data };
    }

    ApiServer listen(const std::string& host, int port, uWS::MoveOnlyFunction<void(bool)>&& handler)
    {
        return { uWS::App::listen(host, port,[data = data, handler = std::move(handler)](auto* socket) mutable {
            if (socket) {
                data->socket = socket;
            }
            handler(socket != nullptr);
        }), data };
    }

    void run()
    {
        data->loop = uWS::Loop::get();
        uWS::run();
    }

    void publish(const std::string& topic, const json& message)
    {
        std::lock_guard<std::mutex> lock(data->mutex);
        if (data->stopped) {
            return;
        }
        data->loop->defer([this, topic, message] {
            uWS::App::publish(topic, message.dump(4), uWS::TEXT);
        });
    }

    static void writeResponse(HttpResponse* res, const json& data);

private:
    std::shared_ptr<ApiServerData> data;
};

}
