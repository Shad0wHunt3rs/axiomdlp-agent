// src/net/http_client.h
//
// HTTP client using cpp-httplib for enrollment, heartbeat, and scan batches.
// Uses OpenSSL for TLS (via CPPHTTPLIB_OPENSSL_SUPPORT).
// TLS is always verified.

#pragma once

#include <string>

namespace axiom::core {

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    struct Response {
        int         status = 0;
        std::string body;
        std::string error;

        bool ok()     const { return error.empty() && status > 0; }
        bool is_2xx() const { return status >= 200 && status < 300; }
    };

    // Synchronous POST with JSON body. Optional Bearer token.
    Response post_json(const std::string& url,
                       const std::string& json_body,
                       const std::string& bearer_token = "",
                       int                timeout_secs = 10);
};

} // namespace axiom::core