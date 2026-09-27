// src/net/http_client.h
//
// HttpClient — a synchronous HTTP client built on Boost.Beast.
//
// Design:
//   - Synchronous (blocking) API. Callers are expected to be on a
//     background thread (e.g. AgentSyncModule's heartbeat loop).
//   - Supports HTTP only (no TLS yet). HTTPS will be added later via
//     Boost.Asio's SSL stream.
//   - Sends JSON bodies with an optional Bearer token.
//   - Never throws. All errors are reported via the Response struct.
//
// The implementation is pimpl'd so this header stays lightweight and
// does not drag Boost.Beast into every translation unit that includes it.

#pragma once

#include <memory>
#include <string>

namespace axiom::core {

class HttpClient {
public:
    // The outcome of one HTTP call.
    struct Response {
        int         status = 0;   // HTTP status code, or 0 on transport error
        std::string body;         // response body (may be empty)
        std::string error;        // transport-level error message (empty on success)

        bool ok() const     { return error.empty() && status > 0; }
        bool is_2xx() const { return status >= 200 && status < 300; }
    };

    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&)                 = delete;
    HttpClient& operator=(HttpClient&&)      = delete;

    // Perform a synchronous POST with a JSON body and an optional
    // bearer token. Blocks until the response arrives or the timeout
    // is exceeded.
    //
    // url            — must start with "http://". E.g. "http://127.0.0.1:3000/api/v1/agent/register"
    // json_body      — the raw JSON string to send
    // bearer_token   — optional; if non-empty, sent as "Authorization: Bearer <token>"
    // timeout_secs   — total time budget for the whole exchange
    Response post_json(const std::string& url,
                       const std::string& json_body,
                       const std::string& bearer_token = "",
                       int timeout_secs = 10);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axiom::core