// src/net/http_client.cpp

#include "net/http_client.h"

#include <spdlog/spdlog.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <exception>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;

namespace axiom::core {

// ----------------------------------------------------------------
// URL parsing (http://host[:port]/path)
// ----------------------------------------------------------------

namespace {

struct ParsedUrl {
    std::string host;
    std::string port   = "80";
    std::string target = "/";
    bool        valid  = false;
};

ParsedUrl parse_http_url(const std::string& url) {
    ParsedUrl r;

    const std::string prefix = "http://";
    if (url.rfind(prefix, 0) != 0) {
        return r;   // not http://
    }

    const std::size_t host_start = prefix.size();
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
// Impl (empty — nothing to hold yet, but keeps the pattern)
// ----------------------------------------------------------------

struct HttpClient::Impl {
    // Stateless for now. Kept so we can add pooled connections or
    // reusable io_context objects later without changing the header.
};

// ----------------------------------------------------------------
// Construction
// ----------------------------------------------------------------

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

// ----------------------------------------------------------------
// POST
// ----------------------------------------------------------------

HttpClient::Response HttpClient::post_json(const std::string& url,
                                           const std::string& json_body,
                                           const std::string& bearer_token,
                                           int timeout_secs) {
    Response resp;

    const ParsedUrl parsed = parse_http_url(url);
    if (!parsed.valid) {
        resp.error = "invalid URL (must start with http://): " + url;
        return resp;
    }

    try {
        net::io_context ioc;

        tcp::resolver     resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Apply the timeout to every I/O operation on this stream.
        stream.expires_after(std::chrono::seconds(timeout_secs));

        const auto results = resolver.resolve(parsed.host, parsed.port);
        stream.connect(results);

        // Build the request.
        http::request<http::string_body> req{http::verb::post, parsed.target, 11};

        // Host header must include the port if it was explicit in the URL.
        std::string host_header = parsed.host;
        if (parsed.port != "80") {
            host_header += ":" + parsed.port;
        }
        req.set(http::field::host, host_header);
        req.set(http::field::user_agent, "axiomdlp-agent/0.1");
        req.set(http::field::content_type, "application/json");

        if (!bearer_token.empty()) {
            req.set(http::field::authorization, "Bearer " + bearer_token);
        }

        req.body() = json_body;
        req.prepare_payload();

        // Send and receive.
        http::write(stream, req);

        beast::flat_buffer                buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        resp.status = res.result_int();
        resp.body   = std::move(res.body());

        // No explicit shutdown or close. tcp_stream's destructor
        // closes the socket cleanly. We've already read the full
        // response body, so there is nothing left to negotiate.
        //
        // (An explicit shutdown() call would also work but trips
        // clang-tidy's bugprone-unused-return-value check on recent
        // Boost versions where the return is marked non-discardable.)

    } catch (const std::exception& e) {
        resp.status = 0;
        resp.error  = e.what();
    }

    return resp;
}

} // namespace axiom::core