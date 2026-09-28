// src/modules/transport/outbound_queue.h
//
// OutboundQueue is a bounded, priority-ordered FIFO. Higher priorities
// are dequeued first. Messages at the same priority preserve FIFO order.
//
// The queue is bounded by both message count and total bytes. When
// either limit is exceeded, the oldest message at the lowest priority
// is dropped. This is deliberate: a slow network must not grow the
// queue without limit.

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

namespace axiom::core {

class OutboundQueue {
public:
    enum class Priority {
        Critical = 0,
        High     = 1,
        Normal   = 2,
        Low      = 3,
        Count    = 4
    };

    OutboundQueue(std::size_t max_messages, std::size_t max_bytes);
    ~OutboundQueue();

    OutboundQueue(const OutboundQueue&) = delete;
    OutboundQueue& operator=(const OutboundQueue&) = delete;

    // Enqueue a message. Returns false if the queue is full and the
    // message was dropped.
    bool push(Priority p, std::string message);

    // Pop the highest-priority message. Blocks until one is available
    // or stop() is called. Returns false if woken by stop().
    bool pop(std::string& out);

    // Non-blocking pop. Returns false immediately if the queue is
    // empty.
    bool try_pop(std::string& out);

    // Wake any blocked pop() callers and mark the queue as stopped.
    void stop();

    std::size_t size() const;

private:
    struct Message {
        Priority    priority;
        std::string payload;
    };

    bool is_empty_locked() const;
    std::size_t count_messages_locked() const;

    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::deque<Message>     queues_[static_cast<int>(Priority::Count)];
    std::size_t             bytes_ = 0;
    std::size_t             max_messages_;
    std::size_t             max_bytes_;
    bool                    stopped_ = false;
};

} // namespace axiom::core