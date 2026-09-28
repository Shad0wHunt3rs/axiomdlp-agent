// src/modules/transport/websocket_client.cpp
//
// Built on Boost.Beast + Boost.Asio, using the asynchronous API.
// Supports ws:// (plain TCP) and wss:// (TLS via OpenSSL).
//
// All I/O runs on a dedicated io_context thread. On stop(),
// io_context::stop() cancels every pending operation.

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
#include <string>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
namespace ssl       = boost::asio::ssl;
using tcp           = net::ip::tcp;
namespace sys       = boost::system;
using namespace std::chrono_literals;

namespace axiom::core {

// ----------------------------------------------------------------
// URL parsing
// ----------------------------------------------------------------

namespace {

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

} // namespace

// ----------------------------------------------------------------
// Stream type aliases
// ----------------------------------------------------------------

using WsPlain = websocket::stream<beast::tcp_stream>;
using WsTls   = websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

// ----------------------------------------------------------------
// Impl
// ----------------------------------------------------------------

struct WebSocketClient::Impl {
    std::string url;
    int initial_backoff_s = 5;
    int max_backoff_s     = 300;
    std::size_t queue_max_messages = 1000;
    std::size_t queue_max_bytes    = 10 * 1024 * 1024;
    bool verify_tls = true;

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

    // TLS context. Declared before the streams so reverse-order
    // destruction destroys the streams first, then the context.
    std::unique_ptr<ssl::context> ssl_ctx;

    // Exactly one of these is non-null while connected.
    std::unique_ptr<WsPlain> ws_plain;
    std::unique_ptr<WsTls>   ws_tls;

    std::unique_ptr<tcp::resolver>     resolver;
    std::unique_ptr<net::steady_timer> reconnect_timer;

    beast::flat_buffer read_buffer;
    std::shared_ptr<std::string> current_write;

    // Serializes send() and stop().
    std::mutex lifecycle_mutex;
};

// ----------------------------------------------------------------
// Construction / destruction
// ----------------------------------------------------------------

WebSocketClient::WebSocketClient()
    : impl_(std::make_unique<Impl>()) {}

WebSocketClient::~WebSocketClient() {
    stop();
}

// ----------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------

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

void WebSocketClient::set_verify_tls(bool verify) {
    impl_->verify_tls = verify;
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

// ----------------------------------------------------------------
// State change
// ----------------------------------------------------------------

void WebSocketClient::set_state(State s) {
    const State old = impl_->state.exchange(s);
    if (old != s && impl_->on_state_change) {
        impl_->on_state_change(s);
    }
}

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------

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
        net::post(*impl_->ioc, [this] { start_connect(); });
        impl_->ioc->run();
        SPDLOG_INFO("WebSocketClient: I/O thread exiting");
        set_state(State::Disconnected);
    });

    return true;
}

void WebSocketClient::stop() {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);

    if (!impl_->running.exchange(false)) {
        return;
    }

    if (impl_->queue) {
        impl_->queue->stop();
    }

    if (impl_->ioc) {
        impl_->ioc->stop();
    }

    if (impl_->io_thread.joinable()) {
        impl_->io_thread.join();
    }

    impl_->reconnect_timer.reset();
    impl_->resolver.reset();
    impl_->ws_plain.reset();
    impl_->ws_tls.reset();
    impl_->ssl_ctx.reset();
    impl_->ioc.reset();

    set_state(State::Disconnected);
}

bool WebSocketClient::send(std::string message, OutboundQueue::Priority priority) {
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);

    if (!impl_->running.load() || !impl_->queue || !impl_->ioc) {
        return false;
    }

    const bool ok = impl_->queue->push(priority, std::move(message));

    if (ok) {
        net::post(*impl_->ioc, [this] { start_write(); });
    }

    return ok;
}

// ----------------------------------------------------------------
// Close helpers
// ----------------------------------------------------------------

void WebSocketClient::close_streams() {
    boost::system::error_code ignored;
    if (impl_->ws_plain) {
        beast::get_lowest_layer(*impl_->ws_plain).socket().close(ignored);
        impl_->ws_plain.reset();
    }
    if (impl_->ws_tls) {
        beast::get_lowest_layer(*impl_->ws_tls).socket().close(ignored);
        impl_->ws_tls.reset();
    }
}

// ----------------------------------------------------------------
// Async state machine — connect
// ----------------------------------------------------------------

void WebSocketClient::start_connect() {
    if (!impl_->running.load()) return;

    set_state(State::Connecting);
    SPDLOG_INFO("WebSocketClient: connecting to {}:{} ({})",
                impl_->host, impl_->port,
                impl_->is_tls ? "wss" : "ws");

    impl_->resolver = std::make_unique<tcp::resolver>(*impl_->ioc);

    impl_->resolver->async_resolve(
        impl_->host, impl_->port,
        [this](sys::error_code ec, tcp::resolver::results_type results) {
            if (is_shutdown_error(ec)) return;
            if (ec) { on_connect_error(ec); return; }

            if (impl_->is_tls) {
                // ---------- TLS path ----------
                impl_->ssl_ctx = std::make_unique<ssl::context>(
                    ssl::context::tls_client);
                impl_->ssl_ctx->set_default_verify_paths();

                impl_->ws_tls = std::make_unique<WsTls>(
                    *impl_->ioc, *impl_->ssl_ctx);

                // SNI — required by virtually every modern server.
                SSL* ssl_handle = impl_->ws_tls->next_layer().native_handle();
                if (!SSL_set_tlsext_host_name(ssl_handle,
                                              impl_->host.c_str())) {
                    on_connect_error(make_error_code(
                        boost::system::errc::invalid_argument));
                    return;
                }

                // Hostname verification via OpenSSL's X509_VERIFY_PARAM.
                if (impl_->verify_tls) {
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
                } else {
                    impl_->ws_tls->next_layer().set_verify_mode(ssl::verify_none);
                    SPDLOG_WARN("WebSocketClient: TLS verification DISABLED for {}",
                                impl_->host);
                }

                // The return value of expires_after is either void or a
                // cancel count depending on the Boost version. We don't
                // use it — we just want the timer set.
                // NOLINTNEXTLINE(bugprone-unused-return-value)
                beast::get_lowest_layer(*impl_->ws_tls).expires_after(15s);

                beast::get_lowest_layer(*impl_->ws_tls).async_connect(
                    results,
                    [this](sys::error_code ec, const tcp::endpoint&) {
                        if (is_shutdown_error(ec)) return;
                        if (ec) { on_connect_error(ec); return; }

                        impl_->ws_tls->next_layer().async_handshake(
                            ssl::stream_base::client,
                            [this](sys::error_code ec) {
                                if (is_shutdown_error(ec)) return;
                                if (ec) { on_connect_error(ec); return; }

                                beast::get_lowest_layer(*impl_->ws_tls)
                                    .expires_never();

                                impl_->ws_tls->set_option(
                                    websocket::stream_base::timeout::suggested(
                                        beast::role_type::client));

                                const std::string host_header =
                                    impl_->host + ":" + impl_->port;

                                impl_->ws_tls->async_handshake(
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
                // ---------- Plain path ----------
                impl_->ws_plain = std::make_unique<WsPlain>(*impl_->ioc);

                // NOLINTNEXTLINE(bugprone-unused-return-value)
                beast::get_lowest_layer(*impl_->ws_plain).expires_after(15s);

                beast::get_lowest_layer(*impl_->ws_plain).async_connect(
                    results,
                    [this](sys::error_code ec, const tcp::endpoint&) {
                        if (is_shutdown_error(ec)) return;
                        if (ec) { on_connect_error(ec); return; }

                        beast::get_lowest_layer(*impl_->ws_plain)
                            .expires_never();

                        impl_->ws_plain->set_option(
                            websocket::stream_base::timeout::suggested(
                                beast::role_type::client));

                        const std::string host_header =
                            impl_->host + ":" + impl_->port;

                        impl_->ws_plain->async_handshake(
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

// ----------------------------------------------------------------
// Async read/write
// ----------------------------------------------------------------

void WebSocketClient::start_read() {
    if (!impl_->running.load()) return;
    if (impl_->state.load() != State::Connected) return;

    impl_->read_buffer.consume(impl_->read_buffer.size());

    if (impl_->ws_tls) {
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        beast::get_lowest_layer(*impl_->ws_tls).expires_after(60s);
        impl_->ws_tls->async_read(
            impl_->read_buffer,
            [this](sys::error_code ec, std::size_t bytes) {
                on_read_complete(ec, bytes);
            });
    } else if (impl_->ws_plain) {
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        beast::get_lowest_layer(*impl_->ws_plain).expires_after(60s);
        impl_->ws_plain->async_read(
            impl_->read_buffer,
            [this](sys::error_code ec, std::size_t bytes) {
                on_read_complete(ec, bytes);
            });
    }
}

void WebSocketClient::on_read_complete(const sys::error_code& ec,
                                       std::size_t /*bytes*/) {
    if (is_shutdown_error(ec)) return;

    if (ec == beast::error::timeout) {
        if (impl_->running.load() && impl_->state.load() == State::Connected) {
            start_read();
        }
        return;
    }

    if (ec) {
        on_disconnect(ec);
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
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        beast::get_lowest_layer(*impl_->ws_tls).expires_after(30s);
        impl_->ws_tls->async_write(
            net::buffer(*impl_->current_write),
            [this](sys::error_code ec, [[maybe_unused]] std::size_t bytes) {
                on_write_complete(ec);
            });
    } else if (impl_->ws_plain) {
        // NOLINTNEXTLINE(bugprone-unused-return-value)
        beast::get_lowest_layer(*impl_->ws_plain).expires_after(30s);
        impl_->ws_plain->async_write(
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
    if (ec) { on_disconnect(ec); return; }

    if (impl_->running.load() && impl_->state.load() == State::Connected) {
        start_write();
    }
}

// ----------------------------------------------------------------
// Error handling
// ----------------------------------------------------------------

void WebSocketClient::on_connect_error(sys::error_code ec) {
    if (is_shutdown_error(ec)) return;

    SPDLOG_WARN("WebSocketClient: connect error: {}", ec.message());
    close_streams();
    schedule_reconnect();
}

void WebSocketClient::on_disconnect(sys::error_code ec) {
    if (is_shutdown_error(ec)) return;

    if (ec != websocket::error::closed) {
        SPDLOG_WARN("WebSocketClient: connection lost: {}", ec.message());
    }

    close_streams();
    schedule_reconnect();
}

void WebSocketClient::schedule_reconnect() {
    if (!impl_->running.load()) return;

    set_state(State::Reconnecting);

    const int delay_s = impl_->current_backoff_s;
    SPDLOG_INFO("WebSocketClient: reconnecting in {}s", delay_s);

    if (!impl_->reconnect_timer) {
        impl_->reconnect_timer =
            std::make_unique<net::steady_timer>(*impl_->ioc);
    }

    const std::size_t cancelled =
        impl_->reconnect_timer->expires_after(std::chrono::seconds(delay_s));

    if (cancelled > 0) {
        SPDLOG_DEBUG("WebSocketClient: cancelled {} pending reconnect wait(s)",
                     cancelled);
    }

    impl_->reconnect_timer->async_wait([this](sys::error_code ec) {
        if (is_shutdown_error(ec)) return;
        if (!impl_->running.load()) return;

        impl_->current_backoff_s =
            std::min(impl_->current_backoff_s * 2, impl_->max_backoff_s);

        start_connect();
    });
}

} // namespace axiom::core