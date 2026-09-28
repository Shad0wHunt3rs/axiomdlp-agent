// src/modules/transport/outbound_queue.cpp

#include "net/outbound_queue.h"

#include <spdlog/spdlog.h>

namespace axiom::core {

OutboundQueue::OutboundQueue(std::size_t max_messages, std::size_t max_bytes)
    : max_messages_(max_messages), max_bytes_(max_bytes) {}

OutboundQueue::~OutboundQueue() {
    stop();
}

bool OutboundQueue::push(Priority p, std::string message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (stopped_) return false;

    const std::size_t msg_size = message.size();

    // Make room: drop oldest low-priority items until we fit.
    while (!queues_[static_cast<int>(Priority::Low)].empty() &&
           (bytes_ + msg_size > max_bytes_ ||
            count_messages_locked() >= max_messages_)) {
        bytes_ -= queues_[static_cast<int>(Priority::Low)].front().payload.size();
        queues_[static_cast<int>(Priority::Low)].pop_front();
    }

    // If we still don't fit, try dropping the next priority up.
    while (!queues_[static_cast<int>(Priority::Normal)].empty() &&
           (bytes_ + msg_size > max_bytes_ ||
            count_messages_locked() >= max_messages_)) {
        bytes_ -= queues_[static_cast<int>(Priority::Normal)].front().payload.size();
        queues_[static_cast<int>(Priority::Normal)].pop_front();
    }

    // If we still don't fit, refuse the new message.
    if (bytes_ + msg_size > max_bytes_ ||
        count_messages_locked() >= max_messages_) {
        return false;
    }

    Message m{p, std::move(message)};
    bytes_ += m.payload.size();
    queues_[static_cast<int>(p)].push_back(std::move(m));

    cv_.notify_one();
    return true;
}

bool OutboundQueue::pop(std::string& out) {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] {
        return stopped_ || !is_empty_locked();
    });

    if (stopped_ && is_empty_locked()) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        if (!queues_[i].empty()) {
            Message& m = queues_[i].front();
            out = std::move(m.payload);
            bytes_ -= out.size();
            queues_[i].pop_front();
            return true;
        }
    }
    return false;
}

bool OutboundQueue::try_pop(std::string& out) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_empty_locked()) return false;

    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        if (!queues_[i].empty()) {
            Message& m = queues_[i].front();
            out = std::move(m.payload);
            bytes_ -= out.size();
            queues_[i].pop_front();
            return true;
        }
    }
    return false;
}

void OutboundQueue::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
    cv_.notify_all();
}

std::size_t OutboundQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_messages_locked();
}

bool OutboundQueue::is_empty_locked() const {
    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        if (!queues_[i].empty()) return false;
    }
    return true;
}

std::size_t OutboundQueue::count_messages_locked() const {
    std::size_t total = 0;
    for (int i = 0; i < static_cast<int>(Priority::Count); ++i) {
        total += queues_[i].size();
    }
    return total;
}

} // namespace axiom::core