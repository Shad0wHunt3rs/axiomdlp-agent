// src/modules/transport/websocket_client.cpp
//
// Built on Boost.Beast + Boost.Asio, using the asynchronous API.
// All I/O runs on a dedicated io_context thread. On stop(),
// io_context::stop() cancels every pending operation, so the thread
// exits within microseconds regardless of network state.

#include "modules/transport/websocket_client.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>

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
// Impl
// ----------------------------------------------------------------

struct WebSocketClient::Impl {
    // Configuration
    std::string url;
    int initial_backoff_s = 5;
    int max_backoff_s     = 300;
    std::size_t queue_max_messages = 1000;
    std::size_t queue_max_bytes    = 10 * 1024 * 1024;

    // State
    std::atomic<State> state{State::Disconnected};
    std::atomic<bool>  running{false};
    std::atomic<bool>  write_in_flight{false};
    int                current_backoff_s = 5;

    // Parsed URL
    std::string host;
    std::string port;
    std::string path;

    // Callbacks
    MessageCallback on_message;
    StateCallback   on_state_change;

    // Queue
    std::unique_ptr<OutboundQueue> queue;

    // Threading
    std::thread io_thread;

    // Asio I/O context — created in start(), destroyed in stop()
    std::unique_ptr<net::io_context> ioc;

    // WebSocket stream and helpers (all owned by the io_context thread)
    std::unique_ptr<websocket::stream<beast::tcp_stream>> ws;
    std::unique_ptr<tcp::resolver>                        resolver;
    std::unique_ptr<net::steady_timer>                    reconnect_timer;

    // Reusable read buffer (one read in flight at a time)
    beast::flat_buffer read_buffer;

    // Current write payload — kept alive until the write completes
    std::shared_ptr<std::string> current_write;

    // ---------------------------------------------------------------
    // FIX: serializes send() and stop() so that stop() can never
    // destroy the io_context while a send() call is dereferencing it.
    //
    // Without this, a send() running on the shipper thread could
    // observe a non-null ioc, then have stop() destroy it before the
    // net::post() line executes. Dereferencing a destroyed unique_ptr
    // is undefined behavior (typically a crash).
    //
    // send() is called a handful of times per second, so an
    // uncontended mutex costs nothing measurable.
    // ---------------------------------------------------------------
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
// State change notification
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

    if (scheme == "wss") {
        SPDLOG_ERROR("WebSocketClient: wss:// (TLS) not supported");
        return false;
    }

    impl_->url = url;

    impl_->queue = std::make_unique<OutboundQueue>(
        impl_->queue_max_messages, impl_->queue_max_bytes);

    impl_->ioc = std::make_unique<net::io_context>();

    impl_->running.store(true);

    impl_->io_thread = std::thread([this] {
        SPDLOG_INFO("WebSocketClient: I/O thread starting");

        // Post the first connect to the io_context. The io_context
        // will not exit run() until stop() is called or there are no
        // more pending operations.
        net::post(*impl_->ioc, [this] { start_connect(); });

        impl_->ioc->run();

        SPDLOG_INFO("WebSocketClient: I/O thread exiting");
        set_state(State::Disconnected);
    });

    return true;
}

void WebSocketClient::stop() {
    // ---------------------------------------------------------------
    // FIX: hold lifecycle_mutex for the entire stop sequence. This
    // guarantees that no send() is mid-execution when we destroy
    // impl_->ioc below.
    // ---------------------------------------------------------------
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);

    if (!impl_->running.exchange(false)) {
        return;
    }

    // Wake anyone blocked on the queue.
    if (impl_->queue) {
        impl_->queue->stop();
    }

    // Cancel all pending async operations. run() returns immediately.
    if (impl_->ioc) {
        impl_->ioc->stop();
    }

    if (impl_->io_thread.joinable()) {
        impl_->io_thread.join();
    }

    // Now safe to destroy Asio objects (owned by the I/O thread).
    impl_->reconnect_timer.reset();
    impl_->resolver.reset();
    impl_->ws.reset();
    impl_->ioc.reset();

    set_state(State::Disconnected);
}

bool WebSocketClient::send(std::string message, OutboundQueue::Priority priority) {
    // ---------------------------------------------------------------
    // FIX: same mutex as stop(). If stop() is running, we block
    // here until it finishes; then we see running == false and
    // return cleanly. If stop() has not started, we complete the
    // entire post() call before stop() can proceed.
    // ---------------------------------------------------------------
    std::lock_guard<std::mutex> lock(impl_->lifecycle_mutex);

    if (!impl_->running.load() || !impl_->queue || !impl_->ioc) {
        return false;
    }

    const bool ok = impl_->queue->push(priority, std::move(message));

    if (ok) {
        // Wake the io_context to pick up the new message.
        net::post(*impl_->ioc, [this] { start_write(); });
    }

    return ok;
}

// ----------------------------------------------------------------
// Async state machine
// ----------------------------------------------------------------

void WebSocketClient::start_connect() {
    if (!impl_->running.load()) return;

    set_state(State::Connecting);
    SPDLOG_INFO("WebSocketClient: connecting to {}:{}", impl_->host, impl_->port);

    impl_->ws       = std::make_unique<websocket::stream<beast::tcp_stream>>(*impl_->ioc);
    impl_->resolver = std::make_unique<tcp::resolver>(*impl_->ioc);

    impl_->resolver->async_resolve(
        impl_->host, impl_->port,
        [this](sys::error_code ec, tcp::resolver::results_type results) {
            if (is_shutdown_error(ec)) return;

            if (ec) {
                on_connect_error(ec);
                return;
            }

            beast::get_lowest_layer(*impl_->ws).expires_after(15s);
            beast::get_lowest_layer(*impl_->ws).async_connect(
                results,
                [this](sys::error_code ec, const tcp::endpoint&) {
                    if (is_shutdown_error(ec)) return;

                    if (ec) {
                        on_connect_error(ec);
                        return;
                    }

                    // Handshake.
                    beast::get_lowest_layer(*impl_->ws).expires_never();
                    impl_->ws->set_option(
                        websocket::stream_base::timeout::suggested(
                            beast::role_type::client));

                    const std::string host_header =
                        impl_->host + ":" + impl_->port;

                    impl_->ws->async_handshake(
                        host_header, impl_->path,
                        [this](sys::error_code ec) {
                            if (is_shutdown_error(ec)) return;

                            if (ec) {
                                on_connect_error(ec);
                                return;
                            }

                            SPDLOG_INFO("WebSocketClient: connected");
                            set_state(State::Connected);
                            impl_->current_backoff_s = impl_->initial_backoff_s;

                            start_read();
                            start_write();
                        });
                });
        });
}

void WebSocketClient::start_read() {
    if (!impl_->running.load()) return;
    if (impl_->state.load() != State::Connected) return;

    impl_->read_buffer.consume(impl_->read_buffer.size());

    // A generous read timeout. If the server is silent for 60s, this
    // returns with a timeout error and we retry — the TCP connection
    // is not necessarily dead.
    beast::get_lowest_layer(*impl_->ws).expires_after(60s);

    impl_->ws->async_read(
        impl_->read_buffer,
        [this](sys::error_code ec, std::size_t /*bytes*/) {
            if (is_shutdown_error(ec)) return;

            if (ec == beast::error::timeout) {
                // No message within the timeout. Restart the read.
                if (impl_->running.load() &&
                    impl_->state.load() == State::Connected) {
                    start_read();
                }
                return;
            }

            if (ec) {
                on_disconnect(ec);
                return;
            }

            const auto data =
                beast::buffers_to_string(impl_->read_buffer.data());

            if (impl_->on_message) {
                try {
                    impl_->on_message(data);
                } catch (const std::exception& e) {
                    SPDLOG_ERROR("WebSocketClient: message handler threw: {}",
                                 e.what());
                }
            }

            if (impl_->running.load() &&
                impl_->state.load() == State::Connected) {
                start_read();
            }
        });
}

void WebSocketClient::start_write() {
    if (!impl_->running.load()) return;
    if (impl_->state.load() != State::Connected) return;
    if (impl_->write_in_flight.exchange(true)) return;

    std::string out;
    if (!impl_->queue || !impl_->queue->try_pop(out)) {
        // Nothing to write. The next send() will post another
        // start_write() call.
        impl_->write_in_flight.store(false);
        return;
    }

    impl_->current_write = std::make_shared<std::string>(std::move(out));

    beast::get_lowest_layer(*impl_->ws).expires_after(30s);

    impl_->ws->async_write(
        net::buffer(*impl_->current_write),
        [this](sys::error_code ec, std::size_t /*bytes*/) {
            impl_->current_write.reset();
            impl_->write_in_flight.store(false);

            if (is_shutdown_error(ec)) return;

            if (ec) {
                on_disconnect(ec);
                return;
            }

            if (impl_->running.load() &&
                impl_->state.load() == State::Connected) {
                start_write();
            }
        });
}

// ----------------------------------------------------------------
// Error handling
// ----------------------------------------------------------------

void WebSocketClient::on_connect_error(sys::error_code ec) {
    if (is_shutdown_error(ec)) return;

    SPDLOG_WARN("WebSocketClient: connect error: {}", ec.message());
    schedule_reconnect();
}

void WebSocketClient::on_disconnect(sys::error_code ec) {
    if (is_shutdown_error(ec)) return;

    if (ec != websocket::error::closed) {
        SPDLOG_WARN("WebSocketClient: connection lost: {}", ec.message());
    }

    // Tear down the stream. The socket close is best-effort.
    if (impl_->ws) {
        sys::error_code ignored;
        beast::get_lowest_layer(*impl_->ws).socket().close(ignored);
    }

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

    // expires_after() is marked [[nodiscard]] by Boost and returns the
    // number of pending async wait operations that were cancelled by
    // the reschedule. In a healthy reconnect flow this is always 0
    // (we only reach here when no wait is active). A non-zero value
    // would indicate a bug in the state machine, so we log it rather
    // than discard it.
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