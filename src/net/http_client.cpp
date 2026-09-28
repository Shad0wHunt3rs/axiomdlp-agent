// src/net/http_client.cpp

#include "net/http_client.h"

#include <spdlog/spdlog.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <chrono>
#include <exception>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
using tcp       = net::ip::tcp;

namespace axiom::core {

// ----------------------------------------------------------------
// URL parsing
// ----------------------------------------------------------------

namespace {

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target = "/";
    bool        valid  = false;

    bool is_https() const { return scheme == "https"; }
};

ParsedUrl parse_url(const std::string& url) {
    ParsedUrl r;

    std::string::size_type scheme_end = 0;
    if (url.rfind("http://", 0) == 0) {
        r.scheme   = "http";
        r.port     = "80";
        scheme_end = 7;
    } else if (url.rfind("https://", 0) == 0) {
        r.scheme   = "https";
        r.port     = "443";
        scheme_end = 8;
    } else {
        return r;
    }

    const std::size_t host_start = scheme_end;
    const std::size_t path_start = url.find('/', host_start);

    std::string host_port;
    if (path_start == std::string::npos) {
        host_port = url.substr(host_start);
        r.target  = "/";
    } else {
        host_port = url.substr(host_start, path_start - host_start);
        r.target  = url.substr(path_start);
    }

    const auto colon = host_port.find(':');
    if (colon == std::string::npos) {
        r.host = host_port;
    } else {
        r.host = host_port.substr(0, colon);
        r.port = host_port.substr(colon + 1);
    }

    r.valid = !r.host.empty() && !r.port.empty() && !r.target.empty();
    return r;
}

} // namespace

// ----------------------------------------------------------------
// Impl
// ----------------------------------------------------------------

struct HttpClient::Impl {};

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

// ----------------------------------------------------------------
// POST
// ----------------------------------------------------------------

HttpClient::Response HttpClient::post_json(const std::string& url,
                                           const std::string& json_body,
                                           const std::string& bearer_token,
                                           int                timeout_secs,
                                           bool               verify_tls) {
    Response resp;

    const ParsedUrl parsed = parse_url(url);
    if (!parsed.valid) {
        resp.error = "invalid URL (must start with http:// or https://): " + url;
        return resp;
    }

    try {
        net::io_context ioc;

        // Build the request once. Same shape for HTTP and HTTPS.
        http::request<http::string_body> req{http::verb::post, parsed.target, 11};

        const bool default_port =
            (parsed.scheme == "http"  && parsed.port == "80") ||
            (parsed.scheme == "https" && parsed.port == "443");

        std::string host_header = parsed.host;
        if (!default_port) {
            host_header += ":" + parsed.port;
        }

        req.set(http::field::host,         host_header);
        req.set(http::field::user_agent,   "axiomdlp-agent/0.1");
        req.set(http::field::content_type, "application/json");

        if (!bearer_token.empty()) {
            req.set(http::field::authorization, "Bearer " + bearer_token);
        }

        req.body() = json_body;
        req.prepare_payload();

        tcp::resolver resolver(ioc);
        const auto results = resolver.resolve(parsed.host, parsed.port);

        if (parsed.is_https()) {
            // -------------------- HTTPS --------------------
            ssl::context ctx{ssl::context::tls_client};
            ctx.set_default_verify_paths();

            beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

            // SNI — required by virtually every modern server.
            if (!SSL_set_tlsext_host_name(stream.native_handle(),
                                          parsed.host.c_str())) {
                resp.error = "failed to set SNI hostname";
                return resp;
            }

            if (verify_tls) {
                // Verify the certificate chain against the system trust store.
                stream.set_verify_mode(ssl::verify_peer);

                // Verify the hostname matches the certificate's CN or SAN.
                // Uses OpenSSL's X509_VERIFY_PARAM directly, which works on
                // every OpenSSL version (Boost's host_name_verification
                // helper does not exist in all Boost releases).
                X509_VERIFY_PARAM* param = SSL_get0_param(stream.native_handle());
                X509_VERIFY_PARAM_set_hostflags(
                    param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
                if (X509_VERIFY_PARAM_set1_host(param,
                                                parsed.host.c_str(),
                                                parsed.host.size()) != 1) {
                    resp.error = "failed to configure hostname verification";
                    return resp;
                }
            } else {
                stream.set_verify_mode(ssl::verify_none);
                SPDLOG_WARN("HttpClient: TLS verification DISABLED for {}",
                            parsed.host);
            }

            beast::get_lowest_layer(stream).expires_after(
                std::chrono::seconds(timeout_secs));
            beast::get_lowest_layer(stream).connect(results);

            stream.handshake(ssl::stream_base::client);

            http::write(stream, req);

            beast::flat_buffer                buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            resp.status = res.result_int();
            resp.body   = std::move(res.body());

            // No explicit TLS shutdown. The ssl_stream destructor closes
            // the session cleanly, and the read above has already consumed
            // the full response body.

        } else {
            // -------------------- HTTP --------------------
            beast::tcp_stream stream(ioc);
            stream.expires_after(std::chrono::seconds(timeout_secs));
            stream.connect(results);

            http::write(stream, req);

            beast::flat_buffer                buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            resp.status = res.result_int();
            resp.body   = std::move(res.body());
        }

    } catch (const std::exception& e) {
        resp.status = 0;
        resp.error  = e.what();
    }

    return resp;
}

} // namespace axiom::core