// src/net/http_client.cpp

#include "net/http_client.h"

#include <httplib.h>
#include <spdlog/spdlog.h>

namespace axiom::core {

HttpClient::HttpClient()  = default;
HttpClient::~HttpClient() = default;

HttpClient::Response HttpClient::post_json(const std::string& url,
                                           const std::string& json_body,
                                           const std::string& bearer_token,
                                           int                timeout_secs) {
    Response resp;

    // --- Parse URL scheme://host[:port]/path ---
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        resp.error = "invalid URL (missing scheme)";
        return resp;
    }

    const std::string scheme = url.substr(0, scheme_end);
    if (scheme != "http" && scheme != "https") {
        resp.error = "unsupported scheme: " + scheme;
        return resp;
    }

    const std::size_t host_start = scheme_end + 3;
    const std::size_t path_start = url.find('/', host_start);

    std::string host_port;
    std::string path;
    if (path_start == std::string::npos) {
        host_port = url.substr(host_start);
        path      = "/";
    } else {
        host_port = url.substr(host_start, path_start - host_start);
        path      = url.substr(path_start);
    }

    std::string host;
    int         port = (scheme == "https") ? 443 : 80;

    const auto colon = host_port.find(':');
    if (colon != std::string::npos) {
        host = host_port.substr(0, colon);
        try {
            port = std::stoi(host_port.substr(colon + 1));
        } catch (...) {
            resp.error = "invalid port in URL";
            return resp;
        }
    } else {
        host = host_port;
    }

    if (host.empty()) {
        resp.error = "invalid URL (empty host)";
        return resp;
    }

    // --- HTTPS ---
    if (scheme == "https") {
        httplib::SSLClient cli(host, port);
        cli.enable_server_certificate_verification(true);
        cli.set_connection_timeout(timeout_secs);
        cli.set_read_timeout(timeout_secs);
        cli.set_write_timeout(timeout_secs);
        cli.set_follow_location(true);

        if (!bearer_token.empty()) {
            cli.set_bearer_token_auth(bearer_token);
        }

        auto res = cli.Post(path.c_str(), json_body, "application/json");
        if (!res) {
            resp.error = "request failed: " + httplib::to_string(res.error());
            return resp;
        }
        resp.status = res->status;
        resp.body   = res->body;
    }
    // --- HTTP ---
    else {
        httplib::Client cli(host, port);
        cli.set_connection_timeout(timeout_secs);
        cli.set_read_timeout(timeout_secs);
        cli.set_write_timeout(timeout_secs);
        cli.set_follow_location(true);

        if (!bearer_token.empty()) {
            cli.set_bearer_token_auth(bearer_token);
        }

        auto res = cli.Post(path.c_str(), json_body, "application/json");
        if (!res) {
            resp.error = "request failed: " + httplib::to_string(res.error());
            return resp;
        }
        resp.status = res->status;
        resp.body   = res->body;
    }

    return resp;
}

} // namespace axiom::core