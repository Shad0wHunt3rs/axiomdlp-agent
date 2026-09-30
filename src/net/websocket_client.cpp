// src/net/websocket_client.cpp
//
// Builds on Boost.Beast + Boost.Asio, asynchronous API.
//
// Liveness is handled by Beast's own keep-alive pings. There is NO
// application-level read deadline.
//
// Reconnect uses exponential backoff with ±20% jitter.
// Close codes are preserved and interpreted.
//
// On stop(), a graceful 1000 Normal close frame is written before the
// socket is torn down, so the server sees a clean close instead of a
// 1006 abnormal closure.
//
// Every call whose return type may be non-void in some Boost versions
// is wrapped in an explicit `(void)` cast to silence clang-tidy's
// bugprone-unused-return-value check.

#include "net/websocket_client.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = boost::asio::ssl;
using tcp           = net::ip::tcp;
namespace sys       = boost::system;
using namespace std::chrono_literals;

namespace axiom::core {

namespace {

// ---- URL parsing --------------------------------------------------

bool parse_ws_url(const std::string& url,
                  std::string& scheme,
                  std::string& host,
                  std::string& port,
                  std::string& path) {
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return false;

    scheme = url.substr(0, scheme_end);
    if (scheme != "ws" && scheme != "wss") return false;

    const std::size_t host_start = scheme_end + 3;

    std::size_t path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        path_start = url.size();
        path = "/";
    } else {
        path = url.substr(path_start);
    }

    const auto colon = url.find(':', host_start);
    if (colon != std::string::npos && colon < path_start) {
        host = url.substr(host_start, colon - host_start);
        port = url.substr(colon + 1, path_start - colon - 1);
    } else {
        host = url.substr(host_start, path_start - host_start);
        port = (scheme == "wss") ? "443" : "80";
    }

    return !host.empty() && !port.empty();
}

bool is_shutdown_error(const sys::error_code& ec) {
    return ec == net::error::operation_aborted
        || ec == net::error::operation_not_supported;
}

// Attach Authorization and User-Agent to the WebSocket handshake.
template <typename WsStream>
void attach_handshake_headers(WsStream& ws, const std::string& token) {
    (void)ws.set_option(websocket::stream_base::decorator(
        [token](websocket::request_type& req) {
            req.set(beast::http::field::user_agent, "axiomdlp-agent/0.1");
            if (!token.empty()) {
                req.set(beast::http::field::authorization,
                        "Bearer " + token);
            }
        }));
}

// Interpret a close code from the WebSocket peer.
// Returns true if the client should attempt to reconnect.
bool should_reconnect_on_close(websocket::close_code code) {
    using cc = websocket::close_code;
    switch (code) {
        case cc::normal:           return true;
        case cc::going_away:       return true;
        case cc::protocol_error:   return false;
        case cc::unknown_data:     return false;
        case cc::bad_payload:      return false;
        case cc::policy_error:     return false;  // auth revoked: STOP
        case cc::too_big:          return true;
        case cc::needs_extension:  return false;
        case cc::internal_error:   return true;
        case cc::service_restart:  return true;
        case cc::try_again_later:  return true;
        default:                   return true;
    }
}

} // namespace

// ---- Stream aliases ------------------------------------------------

using WsPlain = websocket::stream<beast::tcp_stream>;
using WsTls   = websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

// ---- Impl ----------------------------------------------------------

struct WebSocketClient::Impl {
    std::string url;
    std::string bearer_token;
    int initial_backoff_s = 5;
    int max_backoff_s     = 300;
    std::size_t queue_max_messages = 1000;
    std::size_t queue_max_bytes    = 10 * 1024 * 1024;

    std::atomic<State> state{State::Disconnected};
    std::atomic<bool>  running{false};
    std::atomic<bool>  write_in_flight{false};
    int                current_backoff_s = 5;
    bool               is_tls = false;

    std::string host;
    std::string port;
    std::string path;

    MessageCallback on_message;
    StateCallback   on_state_change;

    std::unique_ptr<OutboundQueue> queue;
    std::thread io_thread;

    std::unique_ptr<net::io_context> ioc;
    std::unique_ptr<ssl::context>    ssl_ctx;

    std::unique_ptr<WsPlain> ws_plain;
    std::unique_ptr<WsTls>   ws_tls;

    std::unique_ptr<tcp::resolver>     resolver;
    std::unique_ptr<net::steady_timer> reconnect_timer;

    beast::flat_buffer read_buffer;
    std::shared_ptr<std::string> current_write;

    std::mutex lifecycle_mutex;

    std::mt19937 rng{std::random_device{}()};
};

// ---- Construction --------------------------------------------------

WebSocketClient::WebSocketClient()
    : impl_(std::make_unique<Impl>()) {}

WebSocketClient::~WebSocketClient() {
    stop();
}

// ---- Configuration -------------------------------------------------

void WebSocketClient::set_bearer_token(std::string token) {
    impl_->bearer_token = std::move(token);
}

void WebSocketClient::set_backoff(int initial_seconds, int max_seconds) {
    impl_->initial_backoff_s = initial_seconds;
    impl_->max_backoff_s     = max_seconds;
    impl_->current_backoff_s = initial_seconds;
}

void WebSocketClient::set_queue_limits(std::size_t max_messages,
                                       std::size_t max_bytes) {
    impl_->queue_max_messages = max_messages;
    impl_->queue_max_bytes    = max_bytes;
}

void WebSocketClient::set_on_message(MessageCallback cb) {
    impl_->on_message = std::move(cb);
}

void WebSocketClient::set_on_state_change(StateCallback cb) {
    impl_->on_state_change = std::move(cb);
}

WebSocketClient::State WebSocketClient::state() const {
    return impl_->state.load();
}

bool WebSocketClient::is_running() const {
    return impl_->running.load();
}

void WebSocketClient::set_state(State s) {
    const State old = impl_->state.exchange(s);
    if (old != s && impl_->on_state_change) {
        impl_->on_state_change(s);
    }
}

// ---- Lifecycle -----------------------------------------------------

bool WebSocketClient::start(const std::string& url) {
    if (impl_->running.load()) {
        SPDLOG_WARN("WebSocketClient: already running");
        return false;
    }

    std::string scheme;
    if (!parse_ws_url(url, scheme, impl_->host, impl_->port, impl_->path)) {
        SPDLOG_ERROR("WebSocketClient: invalid URL: {}", url);
        return false;
    }

    impl_->is_tls = (scheme == "wss");
    impl_->url    = url;

    impl_->queue = std::make_unique<OutboundQueue>(
        impl_->queue_max_messages, impl_->queue_max_bytes);

    impl_->ioc = std::make_unique<net::io_context>();
    impl_->running.store(true);

    impl_->io_thread = std::thread([this] {
        SPDLOG_INFO("WebSocketClient: I/O thread starting");
        (void)net::post(*impl_->ioc, [this] { start_connect(); });
        impl_->ioc->run();
        SPDLOG_INFO("WebSocketClient: I/O thread exiting");
        set_state(State::Disconnected);
    });

    return true;
}

void WebSocketClient::stop() {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);

    if (!impl_->running.exchange(false)) return;

    if (impl_->queue) impl_->queue->stop();

    // If we were connected, send a proper 1000 Normal close frame
    // before tearing down, so the server sees a clean close instead
    // of a 1006 abnormal closure.
    if (impl_->ioc && impl_->state.load() == State::Connected) {
        (void)net::post(*impl_->ioc, [this] {
            boost::system::error_code ec;

            // Cancel any pending read/write so close() can proceed.
            if (impl_->ws_plain) {
                (void)beast::get_lowest_layer(*impl_->ws_plain)
                    .socket().cancel(ec);
            } else if (impl_->ws_tls) {
                (void)beast::get_lowest_layer(*impl_->ws_tls)
                    .socket().cancel(ec);
            }
            ec.clear();

            // Tighten timeouts so the close handshake can't hang.
            websocket::stream_base::timeout opt;
            opt.handshake_timeout = 2s;
            opt.idle_timeout      = 2s;
            opt.keep_alive_pings  = false;

            if (impl_->ws_plain) {
                (void)impl_->ws_plain->set_option(opt);
                impl_->ws_plain->close(websocket::close_code::normal, ec);
            } else if (impl_->ws_tls) {
                (void)impl_->ws_tls->set_option(opt);
                impl_->ws_tls->close(websocket::close_code::normal, ec);
            }

            if (ec && ec != websocket::error::closed) {
                SPDLOG_DEBUG("WebSocketClient: close handshake incomplete: {}",
                             ec.message());
            } else {
                SPDLOG_INFO("WebSocketClient: sent graceful close (code=1000)");
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    if (impl_->ioc) impl_->ioc->stop();
    if (impl_->io_thread.joinable()) impl_->io_thread.join();

    impl_->reconnect_timer.reset();
    impl_->resolver.reset();
    impl_->ws_plain.reset();
    impl_->ws_tls.reset();
    impl_->ssl_ctx.reset();
    impl_->ioc.reset();
    impl_->write_in_flight.store(false);
    impl_->current_write.reset();

    set_state(State::Disconnected);
}

bool WebSocketClient::send(std::string message, OutboundQueue::Priority priority) {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);

    if (!impl_->running.load() || !impl_->queue || !impl_->ioc) {
        return false;
    }

    const bool ok = impl_->queue->push(priority, std::move(message));

    if (ok) {
        (void)net::post(*impl_->ioc, [this] { start_write(); });
    }

    return ok;
}

void WebSocketClient::close_streams() {
    boost::system::error_code ignored;
    if (impl_->ws_plain) {
        (void)beast::get_lowest_layer(*impl_->ws_plain)
            .socket().close(ignored);
        impl_->ws_plain.reset();
    }
    if (impl_->ws_tls) {
        (void)beast::get_lowest_layer(*impl_->ws_tls)
            .socket().close(ignored);
        impl_->ws_tls.reset();
    }
    impl_->write_in_flight.store(false);
    impl_->current_write.reset();
}

// ---- Connect -------------------------------------------------------

void WebSocketClient::start_connect() {
    if (!impl_->running.load()) return;

    set_state(State::Connecting);
    SPDLOG_INFO("WebSocketClient: connecting to {}:{} ({})",
                impl_->host, impl_->port,
                impl_->is_tls ? "wss" : "ws");

    impl_->resolver = std::make_unique<tcp::resolver>(*impl_->ioc);

    (void)impl_->resolver->async_resolve(
        impl_->host, impl_->port,
        [this](sys::error_code ec, tcp::resolver::results_type results) {
            if (is_shutdown_error(ec)) return;
            if (ec) { on_connect_error(ec); return; }

            if (impl_->is_tls) {
                // -------- TLS --------
                impl_->ssl_ctx = std::make_unique<ssl::context>(
                    ssl::context::tls_client);
                impl_->ssl_ctx->set_default_verify_paths();

                impl_->ws_tls = std::make_unique<WsTls>(
                    *impl_->ioc, *impl_->ssl_ctx);

                SSL* ssl_handle = impl_->ws_tls->next_layer().native_handle();

                if (!SSL_set_tlsext_host_name(ssl_handle,
                                              impl_->host.c_str())) {
                    on_connect_error(make_error_code(
                        boost::system::errc::invalid_argument));
                    return;
                }

                impl_->ws_tls->next_layer().set_verify_mode(ssl::verify_peer);

                X509_VERIFY_PARAM* param = SSL_get0_param(ssl_handle);
                X509_VERIFY_PARAM_set_hostflags(
                    param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
                if (X509_VERIFY_PARAM_set1_host(param,
                                                impl_->host.c_str(),
                                                impl_->host.size()) != 1) {
                    on_connect_error(make_error_code(
                        boost::system::errc::invalid_argument));
                    return;
                }

                attach_handshake_headers(*impl_->ws_tls, impl_->bearer_token);

                (void)beast::get_lowest_layer(*impl_->ws_tls)
                    .expires_after(15s);

                (void)beast::get_lowest_layer(*impl_->ws_tls).async_connect(
                    results,
                    [this](sys::error_code ec, const tcp::endpoint&) {
                        if (is_shutdown_error(ec)) return;
                        if (ec) { on_connect_error(ec); return; }

                        (void)impl_->ws_tls->next_layer().async_handshake(
                            ssl::stream_base::client,
                            [this](sys::error_code ec) {
                                if (is_shutdown_error(ec)) return;
                                if (ec) { on_connect_error(ec); return; }

                                (void)beast::get_lowest_layer(*impl_->ws_tls)
                                    .expires_never();

                                websocket::stream_base::timeout opt =
                                    websocket::stream_base::timeout::suggested(
                                        beast::role_type::client);
                                opt.idle_timeout     = 90s;
                                opt.keep_alive_pings = true;
                                (void)impl_->ws_tls->set_option(opt);

                                const std::string host_header =
                                    impl_->host + ":" + impl_->port;

                                (void)impl_->ws_tls->async_handshake(
                                    host_header, impl_->path,
                                    [this](sys::error_code ec) {
                                        if (is_shutdown_error(ec)) return;
                                        if (ec) { on_connect_error(ec); return; }

                                        SPDLOG_INFO("WebSocketClient: connected (wss)");
                                        set_state(State::Connected);
                                        impl_->current_backoff_s = impl_->initial_backoff_s;
                                        start_read();
                                        start_write();
                                    });
                            });
                    });

            } else {
                // -------- Plain --------
                impl_->ws_plain = std::make_unique<WsPlain>(*impl_->ioc);

                attach_handshake_headers(*impl_->ws_plain, impl_->bearer_token);

                (void)beast::get_lowest_layer(*impl_->ws_plain)
                    .expires_after(15s);

                (void)beast::get_lowest_layer(*impl_->ws_plain).async_connect(
                    results,
                    [this](sys::error_code ec, const tcp::endpoint&) {
                        if (is_shutdown_error(ec)) return;
                        if (ec) { on_connect_error(ec); return; }

                        (void)beast::get_lowest_layer(*impl_->ws_plain)
                            .expires_never();

                        websocket::stream_base::timeout opt =
                            websocket::stream_base::timeout::suggested(
                                beast::role_type::client);
                        opt.idle_timeout     = 90s;
                        opt.keep_alive_pings = true;
                        (void)impl_->ws_plain->set_option(opt);

                        const std::string host_header =
                            impl_->host + ":" + impl_->port;

                        (void)impl_->ws_plain->async_handshake(
                            host_header, impl_->path,
                            [this](sys::error_code ec) {
                                if (is_shutdown_error(ec)) return;
                                if (ec) { on_connect_error(ec); return; }

                                SPDLOG_INFO("WebSocketClient: connected (ws)");
                                set_state(State::Connected);
                                impl_->current_backoff_s = impl_->initial_backoff_s;
                                start_read();
                                start_write();
                            });
                    });
            }
        });
}

// ---- Read/Write ----------------------------------------------------

void WebSocketClient::start_read() {
    if (!impl_->running.load()) return;
    if (impl_->state.load() != State::Connected) return;

    impl_->read_buffer.consume(impl_->read_buffer.size());

    if (impl_->ws_tls) {
        (void)impl_->ws_tls->async_read(
            impl_->read_buffer,
            [this](sys::error_code ec, std::size_t bytes) {
                on_read_complete(ec, bytes);
            });
    } else if (impl_->ws_plain) {
        (void)impl_->ws_plain->async_read(
            impl_->read_buffer,
            [this](sys::error_code ec, std::size_t bytes) {
                on_read_complete(ec, bytes);
            });
    }
}

void WebSocketClient::on_read_complete(const sys::error_code& ec,
                                       std::size_t /*bytes*/) {
    if (is_shutdown_error(ec)) return;

    if (ec) {
        on_disconnect(ec, true);
        return;
    }

    const auto data = beast::buffers_to_string(impl_->read_buffer.data());

    if (impl_->on_message) {
        try {
            impl_->on_message(data);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("WebSocketClient: message handler threw: {}", e.what());
        }
    }

    if (impl_->running.load() && impl_->state.load() == State::Connected) {
        start_read();
    }
}

void WebSocketClient::start_write() {
    if (!impl_->running.load()) return;
    if (impl_->state.load() != State::Connected) return;
    if (impl_->write_in_flight.exchange(true)) return;

    std::string out;
    if (!impl_->queue || !impl_->queue->try_pop(out)) {
        impl_->write_in_flight.store(false);
        return;
    }

    impl_->current_write = std::make_shared<std::string>(std::move(out));

    if (impl_->ws_tls) {
        (void)impl_->ws_tls->async_write(
            net::buffer(*impl_->current_write),
            [this](sys::error_code ec, [[maybe_unused]] std::size_t bytes) {
                on_write_complete(ec);
            });
    } else if (impl_->ws_plain) {
        (void)impl_->ws_plain->async_write(
            net::buffer(*impl_->current_write),
            [this](sys::error_code ec, [[maybe_unused]] std::size_t bytes) {
                on_write_complete(ec);
            });
    } else {
        impl_->write_in_flight.store(false);
    }
}

void WebSocketClient::on_write_complete(const sys::error_code& ec) {
    impl_->current_write.reset();
    impl_->write_in_flight.store(false);

    if (is_shutdown_error(ec)) return;
    if (ec) { on_disconnect(ec, true); return; }

    if (impl_->running.load() && impl_->state.load() == State::Connected) {
        start_write();
    }
}

// ---- Error handling -----------------------------------------------

void WebSocketClient::on_connect_error(sys::error_code ec) {
    if (is_shutdown_error(ec)) return;

    // Boost.Beast does not expose a portable `handshake_declined`
    // error code across versions. The message is stable, so match on
    // it and emit a more actionable warning when we see it.
    const std::string msg = ec.message();
    const bool is_declined =
        msg.find("declined")   != std::string::npos ||
        msg.find("handshake")  != std::string::npos;

    if (is_declined) {
        SPDLOG_WARN("WebSocketClient: connect error: {} — "
                    "the server rejected the WebSocket upgrade. "
                    "This usually means the API token is invalid or revoked, "
                    "or the WebSocket endpoint is disabled on the server.",
                    msg);
    } else {
        SPDLOG_WARN("WebSocketClient: connect error: {}", msg);
    }

    close_streams();
    schedule_reconnect();
}

void WebSocketClient::on_disconnect(sys::error_code ec, bool allow_reconnect) {
    if (is_shutdown_error(ec)) return;

    websocket::close_code code = websocket::close_code::none;
    std::string            reason;

    // close_reason::code is std::uint16_t in some Boost versions and
    // close_code in others. static_cast handles both safely.
    if (impl_->ws_plain) {
        auto cr = impl_->ws_plain->reason();
        code   = static_cast<websocket::close_code>(cr.code);
        reason = cr.reason.c_str();
    } else if (impl_->ws_tls) {
        auto cr = impl_->ws_tls->reason();
        code   = static_cast<websocket::close_code>(cr.code);
        reason = cr.reason.c_str();
    }

    if (ec == websocket::error::closed) {
        SPDLOG_INFO("WebSocketClient: server closed connection "
                    "(code={}, reason='{}')",
                    static_cast<unsigned>(code), reason);
    } else {
        SPDLOG_WARN("WebSocketClient: connection lost: {}", ec.message());
    }

    close_streams();

    if (!allow_reconnect || !should_reconnect_on_close(code)) {
        SPDLOG_ERROR("WebSocketClient: close code {} is not retryable — "
                     "stopping permanently "
                     "(server likely revoked this agent's token)",
                     static_cast<unsigned>(code));
        impl_->running.store(false);
        if (impl_->ioc) impl_->ioc->stop();
        return;
    }

    schedule_reconnect();
}

// ---- Reconnect with jitter ----------------------------------------

int WebSocketClient::next_backoff_with_jitter() {
    const int base = impl_->current_backoff_s;
    const int jit  = std::max(1, base / 5); // ±20%
    std::uniform_int_distribution<int> dist(-jit, jit);
    return std::max(1, base + dist(impl_->rng));
}

void WebSocketClient::schedule_reconnect() {
    if (!impl_->running.load()) return;

    set_state(State::Reconnecting);

    const int delay_s = next_backoff_with_jitter();
    SPDLOG_INFO("WebSocketClient: reconnecting in {}s "
                "(base={}s, max={}s)",
                delay_s, impl_->current_backoff_s, impl_->max_backoff_s);

    if (!impl_->reconnect_timer) {
        impl_->reconnect_timer =
            std::make_unique<net::steady_timer>(*impl_->ioc);
    }

    (void)impl_->reconnect_timer
        ->expires_after(std::chrono::seconds(delay_s));

    (void)impl_->reconnect_timer->async_wait([this](sys::error_code ec) {
        if (is_shutdown_error(ec)) return;
        if (!impl_->running.load()) return;

        impl_->current_backoff_s =
            std::min(impl_->current_backoff_s * 2, impl_->max_backoff_s);

        start_connect();
    });
}

} // namespace axiom::core