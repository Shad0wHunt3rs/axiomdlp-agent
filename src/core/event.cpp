// src/core/event.cpp

#include "core/event.h"

#include <atomic>

namespace axiom::core {

namespace {

// Monotonic counter so two events created in the same nanosecond still
// have distinct IDs.
std::atomic<std::uint64_t> g_counter{0};

std::string generate_id() {
    const auto now = std::chrono::system_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    const auto c = g_counter.fetch_add(1, std::memory_order_relaxed);
    return std::to_string(ns) + "-" + std::to_string(c);
}

} // namespace

Event Event::create(const std::string& type) {
    Event e;
    e.id        = generate_id();
    e.type      = type;
    e.timestamp = std::chrono::system_clock::now();
    return e;
}

} // namespace axiom::core
