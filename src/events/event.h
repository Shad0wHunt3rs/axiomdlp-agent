// src/core/event.h
//
// The Event struct. Every interesting thing that happens in the agent
// becomes an Event that is published on the event bus.
//
// Events are passed by shared_ptr so multiple subscribers can receive
// the same event without copying it.

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <string>

namespace axiom::core {

struct Event {
    // Unique identifier (generated at creation).
    std::string id;

    // Type of the event, e.g. "file_access", "usb_insert".
    // Subscribers filter by this string.
    std::string type;

    // When the event occurred.
    std::chrono::system_clock::time_point timestamp;

    // Optional attribution.
    std::string user_id;
    std::string process_name;
    int         pid = 0;

    // Arbitrary key/value metadata (path, size, action, etc.).
    std::map<std::string, std::string> metadata;

    // Optional payload (text extracted from a file, clipboard, etc.).
    std::optional<std::string> content;

    // Create a new event of the given type with a fresh ID and
    // the current timestamp.
    static Event create(const std::string& type);
};

// Canonical event type strings. Using these avoids typos and gives
// the compiler something to check against.
namespace event_types {

inline constexpr const char* kFileAccess    = "file_access";
inline constexpr const char* kFileWrite     = "file_write";
inline constexpr const char* kFileDelete    = "file_delete";
inline constexpr const char* kUsbInsert     = "usb_insert";
inline constexpr const char* kUsbRemove     = "usb_remove";
inline constexpr const char* kClipboardCopy = "clipboard_copy";
inline constexpr const char* kPrintJob      = "print_job";
inline constexpr const char* kProcessSpawn  = "process_spawn";
inline constexpr const char* kScanResult    = "scan_result";
inline constexpr const char* kDecision      = "decision";

// Transport lifecycle. Published by the transport module on every
// connect / disconnect. `metadata["state"]` is "connected" or
// "disconnected"; `metadata["source"]` is the transport kind
// ("mqtt_transport").
inline constexpr const char* kMqttState     = "mqtt_state";

} // namespace event_types

} // namespace axiom::core