// src/modules/transport/websocket_client.h
//
// WebSocketClient manages a persistent WebSocket connection to the
// AxiomDLP Core server. It runs on a single dedicated Asio io_context
// thread and exposes a thread-safe send() method plus callbacks for
// received messages and state changes.
//
// Built on Boost.Beast + Boost.Asio, using the asynchronous API. All
// I/O is non-blocking; stop() cancels pending operations immediately
// via io_context::stop().
//
// This class knows nothing about JSON payloads, the event bus, or the
// module system. It moves bytes. The TransportModule gives those bytes
// meaning.

#pragma once

#include "modules/transport/outbound_queue.h"

#include <boost/system/error_code.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace axiom::core {

class WebSocketClient {
public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting
    };

    using MessageCallback = std::function<void(std::string_view)>;
    using StateCallback   = std::function<void(State)>;

    WebSocketClient();
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    // Start the I/O thread. Returns false only if the URL is malformed
    // or the thread cannot be created. Connection failures are retried
    // in the background.
    bool start(const std::string& url);

    // Stop the I/O thread and close the connection. Blocks until the
    // thread exits. Safe to call multiple times.
    void stop();

    // Enqueue a message for delivery. Thread-safe. Returns false if the
    // queue is full (message dropped).
    bool send(std::string message,
              OutboundQueue::Priority priority = OutboundQueue::Priority::Normal);

    // Set reconnect backoff bounds. Call before start().
    void set_backoff(int initial_seconds, int max_seconds);

    // Set queue limits. Call before start().
    void set_queue_limits(std::size_t max_messages, std::size_t max_bytes);

    // Callbacks must be set before start().
    void set_on_message(MessageCallback cb);
    void set_on_state_change(StateCallback cb);

    State state() const;
    bool  is_running() const;

private:
    void set_state(State s);

    // Async state machine — all run on the io_context thread.
    void start_connect();
    void start_read();
    void start_write();
    void on_connect_error(boost::system::error_code ec);
    void on_disconnect(boost::system::error_code ec);
    void schedule_reconnect();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axiom::core