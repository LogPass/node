#include "pch.h"

#include "api_server.h"

namespace logpass {

void ApiServer::writeResponse(HttpResponse* res, const json& data) {
    std::string status, error, response;
    try {
        if (data.is_number_integer()) {
            int errorCode = data.get<int>();
            if (errorCode == 400) {
                status = "400 Bad Request";
                error = "Bad Request";
            } else if (errorCode == 404) {
                status = "404 Not Found";
                error = "Not Found";
            } else if (errorCode == 500) {
                status = "500 Internal Server Error";
                error = "Internal Server Error";
            } else {
                status = "500 Internal Server Error";
                error = "Unknown error: "s + std::to_string(errorCode);
            }
        } else {
            response = data.dump(4);
        }
    } catch (const json::exception&) {
        status = "500 Internal Server Error";
        error = "Exception";
    }

    if (!error.empty()) {
        response = json{ {"error", error } }.dump(4);
    }

    res->cork([res, status = std::move(status), response = std::move(response)] {
        if (!status.empty()) {
            res->writeStatus(status);
        }
        res->writeHeader("Content-Type", "application/json");
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Access-Control-Allow-Methods", "*");
        res->writeHeader("Access-Control-Allow-Credentials", "true");
        res->writeHeader("Cross-Origin-Resource-Policy", "cross-origin");
        res->end(response);
    });
}

}
