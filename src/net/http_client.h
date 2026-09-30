// src/net/http_client.h
//
// HttpClient — a synchronous HTTP/HTTPS client built on Boost.Beast.
//
//   - Synchronous (blocking) API, called from background threads.
//   - Supports http:// and https://.
//   - Sends JSON bodies with an optional Bearer token.
//   - TLS verification is ALWAYS enabled when the URL is https://.
//     There is no opt-out: certificate chain and hostname must match.
//   - Never throws. All errors are reported via the Response struct.

#pragma once

#include <memory>
#include <string>

namespace axiom::core {

class HttpClient {
public:
    struct Response {
        int         status = 0;
        std::string body;
        std::string error;

        bool ok() const     { return error.empty() && status > 0; }
        bool is_2xx() const { return status >= 200 && status < 300; }
    };

    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&)                 = delete;
    HttpClient& operator=(HttpClient&&)      = delete;

    // Synchronous POST. Blocks until the response arrives or the
    // timeout is exceeded.
    //
    // url          — "http://..." or "https://..."
    // json_body    — raw JSON string
    // bearer_token — optional; sent as "Authorization: Bearer <token>"
    // timeout_secs — total time budget
    Response post_json(const std::string& url,
                       const std::string& json_body,
                       const std::string& bearer_token = "",
                       int                timeout_secs = 10);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axiom::core