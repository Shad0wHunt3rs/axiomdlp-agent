// src/net/websocket_client.h
//
// Persistent WebSocket client for AxiomDLP Core. Supports ws:// and wss://.
// TLS is always verified on wss://.
//
// Liveness is handled by Beast's own keep-alive pings. There is NO
// application-level read deadline: a WebSocket can be idle for hours
// and remain valid, so a per-read timer is incorrect.
//
// Reconnect uses exponential backoff with ±20% jitter.

#pragma once

#include "net/outbound_queue.h"

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

    // Bearer token sent on the handshake. Call before start().
    void set_bearer_token(std::string token);

    bool start(const std::string& url);
    void stop();

    bool send(std::string message,
              OutboundQueue::Priority priority = OutboundQueue::Priority::Normal);

    void set_backoff(int initial_seconds, int max_seconds);
    void set_queue_limits(std::size_t max_messages, std::size_t max_bytes);

    void set_on_message(MessageCallback cb);
    void set_on_state_change(StateCallback cb);

    State state() const;
    bool  is_running() const;

private:
    void set_state(State s);

    void start_connect();
    void start_read();
    void start_write();

    void on_read_complete(const boost::system::error_code& ec, std::size_t bytes);
    void on_write_complete(const boost::system::error_code& ec);
    void on_connect_error(boost::system::error_code ec);
    void on_disconnect(boost::system::error_code ec, bool allow_reconnect);
    void schedule_reconnect();
    void close_streams();
    int  next_backoff_with_jitter();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace axiom::core