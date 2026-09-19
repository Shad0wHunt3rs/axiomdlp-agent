# What To Do Next — Implementation Roadmap

This document describes the work ahead in full detail. It is a working document — each milestone is described well enough to be implemented, tested, and committed.

---

## Table of Contents

1. [Where We Are Now](#1-where-we-are-now)
2. [Milestone 5 — Module System](#2-milestone-5--module-system)
3. [Milestone 6 — Event Bus](#3-milestone-6--event-bus)
4. [Milestone 7 — Filesystem Module](#4-milestone-7--filesystem-module)
5. [Milestone 8 — Scanner](#5-milestone-8--scanner)
6. [Milestone 9 — Policy Engine](#6-milestone-9--policy-engine)
7. [Milestone 10 — Storage](#7-milestone-10--storage)
8. [Milestone 11 — User-Mode Enforcement](#8-milestone-11--user-mode-enforcement)
9. [Milestone 12 — IPC + User Helper](#9-milestone-12--ipc--user-helper)
10. [Milestone 13 — Transport](#10-milestone-13--transport)
11. [Milestone 14 — Kernel Enforcement](#11-milestone-14--kernel-enforcement)
12. [Ordering Rationale](#12-ordering-rationale)
13. [Working Agreement](#13-working-agreement)

---

## 1. Where We Are Now

### What Works

- Cross-compiled binaries for Linux 32/64 and Windows 32/64
- Async structured logging with rotation
- Config loading from JSON with validation
- Service lifecycle: install, start, stop, uninstall on both platforms
- Graceful shutdown via signals (Linux) and SCM controls (Windows)

### What Doesn't Exist

- No modules — nothing to monitor or scan
- No event bus — no way for subsystems to communicate
- No policy engine — no decisions
- No storage — no persistence
- No transport — no connection to the core

**The foundation is complete. The DLP features begin now.**

---

## 2. Milestone 5 — Module System

**Goal:** Create the plugin architecture that every future feature will use.

### 2.1 What It Delivers

- `IModule` interface — the contract for all modules
- `ModuleManager` — orchestrates module lifecycle
- `ServiceContext` — shared dependency container
- A dummy "example" module to prove the pattern
- Integration with `Application` and `LinuxService` / `WindowsService`

### 2.2 New Files

```
src/
├── core/
│   ├── module.h                 (interface)
│   ├── module_manager.{h,cpp}   (orchestrator)
│   ├── service_context.{h,cpp}  (shared dependencies)
│   └── health.h                 (health status types)
└── modules/
    ├── module_base.{h,cpp}      (common base class)
    └── example/
        ├── example_module.{h,cpp}
        └── CMakeLists.txt
```

### 2.3 The `IModule` Interface

Each module must implement:

| Method | Signature | Purpose |
|--------|-----------|---------|
| `name` | `std::string name() const` | Module identifier |
| `version` | `std::string version() const` | Semantic version |
| `initialize` | `bool initialize(ServiceContext&)` | One-time setup with dependencies |
| `start` | `bool start()` | Begin work |
| `stop` | `void stop()` | Stop work |
| `shutdown` | `void shutdown()` | Release resources |
| `health` | `HealthStatus health() const` | Current status |

**Why `bool` on initialize and start:** They can fail. A module that can't initialize should report this, and the manager should log it and continue with others.

**Why `void` on stop and shutdown:** Best-effort operations. Errors are logged but not propagated.

**Why `health` returns a value:** The manager needs a snapshot of each module's state.

### 2.4 Health Status

```cpp
enum class HealthLevel {
    OK,        // Fully working
    Degraded,  // Working but with issues
    Failed,    // Not working
    Unknown,   // Not yet checked
};

struct HealthStatus {
    HealthLevel level = HealthLevel::Unknown;
    std::string message;   // Human-readable detail
};
```

**Why four levels:** A simple OK/Failed doesn't capture partial failures. Degraded covers "working but slow" or "partial functionality."

### 2.5 The `ServiceContext`

A struct holding references to shared subsystems:

```cpp
struct ServiceContext {
    Config&      config;
    Logger&      logger;    // (future: if we refactor logging)
    // Future: EventBus& event_bus;
    // Future: Storage& storage;
    // Future: PolicyEngine& policy_engine;
    // Future: Transport& transport;
};
```

**Why references, not pointers:** The context owns nothing. It points to objects that live in the `Application`. Non-null by construction.

**Why it grows over time:** New subsystems are added as fields. Modules receive the whole context; they only use what they need.

### 2.6 The `ModuleManager`

Responsibilities:

- Store a list of registered modules (via `std::vector<std::unique_ptr<IModule>>`)
- `initialize_all(ServiceContext&)` — initialize each module, log failures
- `start_all()` — start each module in order
- `stop_all()` — stop each module in reverse order
- `shutdown_all()` — shutdown each module in reverse order
- `health_all()` — query each module's health, return aggregate
- `register_module(std::unique_ptr<IModule>)` — add to the list

**Why reverse order for stop/shutdown:** LIFO. If module B depends on module A being started (e.g., B subscribes to events from A), then B must stop before A. Reverse order guarantees this.

**Why failures don't stop the manager:** If one module fails to start, the manager logs it and continues with the others. One broken module must never bring down the agent.

### 2.7 The Example Module

A minimal module that:

- Logs `"example module initialized"` in `initialize`
- Logs `"example module started"` in `start`
- Sleeps or does trivial work in a background thread (optional)
- Logs `"example module stopped"` in `stop`
- Logs `"example module shutdown"` in `shutdown`
- Reports `HealthLevel::OK` in `health`

**Purpose:** Proves the pattern. Future modules copy this structure.

### 2.8 Integration With Application

The `Application` creates the `ModuleManager`, registers all modules, and passes it to the service.

```
Application::execute()
  ├── init() → load config, init logging
  ├── create ModuleManager
  ├── register ExampleModule
  ├── create ServiceContext
  ├── create Service (platform-specific)
  ├── service->set_module_manager(...)
  ├── service->set_context(...)
  └── service->run()
```

### 2.9 Integration With Service

The service's `run()` method gets three new lines:

```
Service::run()
  ├── install signal handlers
  ├── module_manager.initialize_all(context)
  ├── module_manager.start_all()
  ├── sd_notify(READY=1) / report(RUNNING)
  ├── while (!stop) { sleep; }
  ├── module_manager.stop_all()
  ├── module_manager.shutdown_all()
  └── sd_notify(STOPPING=1) / report(STOPPED)
```

### 2.10 Success Criteria

When this milestone is done:

1. The agent starts and shows four lines from the example module
2. The agent stops cleanly and shows the stop + shutdown lines
3. Adding a second module requires only one line of registration in `Application`
4. The service loop is unchanged — three lines added, nothing restructured
5. All four targets build cleanly

---

## 3. Milestone 6 — Event Bus

**Goal:** Enable modules to communicate via publish/subscribe without knowing about each other.

### 3.1 What It Delivers

- `Event` struct — the universal data type flowing through the agent
- `EventBus` class — in-process pub/sub
- Integration with `ServiceContext` (modules receive it)
- A subscriber that logs every event (proof of concept)
- The example module publishes dummy events

### 3.2 The `Event` Struct

```cpp
struct Event {
    std::string id;                    // UUID
    std::string type;                  // "file_access", "usb_insert", ...
    std::chrono::system_clock::time_point timestamp;
    std::string user_id;
    std::string process_name;
    int pid;
    std::map<std::string, std::string> metadata;
    std::optional<std::string> content;      // text to scan
    std::optional<std::vector<uint8_t>> blob; // binary data
};
```

**Why this shape:**

- **id** — every event is uniquely identifiable
- **type** — a string, not an enum. New event types can be added without recompiling subscribers.
- **timestamp** — required for auditing and ordering
- **user_id, process_name, pid** — attribution
- **metadata** — arbitrary key-value pairs for module-specific data
- **content/blob** — optional; only set for events that carry payloads

**Event types** (declared as constants in `event_types.h`):

- `file_access`
- `file_write`
- `usb_insert`
- `usb_remove`
- `clipboard_copy`
- `clipboard_paste`
- `print_job`
- `browser_upload`
- `process_spawn`

### 3.3 The `EventBus` Class

```cpp
class EventBus {
public:
    using Handler = std::function<void(std::shared_ptr<Event>)>;

    void subscribe(const std::string& event_type, Handler handler);
    void subscribe_all(Handler handler);
    void publish(std::shared_ptr<Event> event);
};
```

**Why `shared_ptr<Event>`:** Multiple subscribers may receive the same event. Shared ownership avoids copies.

**Why `std::function`:** Handlers can be lambdas, free functions, or bound member functions. Maximum flexibility.

**Why both `subscribe` (typed) and `subscribe_all`:** Modules usually care about specific events, but a logger or audit system wants everything.

### 3.4 Threading

The event bus is called from multiple threads. All `subscribe` and `publish` operations must be thread-safe.

- `std::mutex` guards the subscriber list
- Handlers run on the publishing thread (or a worker thread — decision below)

**Design choice: synchronous vs asynchronous publishing**

- **Synchronous:** `publish` calls each handler directly. Fast, simple, but a slow handler blocks the publisher.
- **Asynchronous:** `publish` pushes to a queue. A worker thread drains it and calls handlers. Never blocks the publisher.

**Recommendation:** Start synchronous. The event bus is called from the file watcher's thread. If a handler blocks, that specific watcher stalls, but the agent continues. Later, if this becomes a bottleneck, switch to async with a queue.

### 3.5 Integration With ServiceContext

```cpp
struct ServiceContext {
    Config&   config;
    EventBus& event_bus;   // NEW
};
```

Modules get the bus through the context. They call `context.event_bus.publish(...)`.

### 3.6 Success Criteria

1. Example module publishes a dummy event every 2 seconds
2. A subscriber logs each event
3. The event contains a timestamp, type, and metadata
4. No deadlocks or crashes under concurrent publishing

---

## 4. Milestone 7 — Filesystem Module

**Goal:** Detect filesystem activity and emit events. This is the first real DLP feature.

### 4.1 What It Delivers

- `FilesystemModule` — a real module implementing `IModule` and using the `EventBus`
- Platform implementations:
  - **Linux:** `inotify` (or `fanotify` for permission events)
  - **Windows:** `ReadDirectoryChangesW`
- Config-driven watch paths
- Events emitted on file create, modify, delete, open

### 4.2 New Files

```
src/
└── modules/
    └── filesystem/
        ├── filesystem_module.{h,cpp}
        ├── CMakeLists.txt
        └── watcher/
            ├── watcher.h            (interface)
            ├── watcher_linux.cpp    (inotify)
            └── watcher_windows.cpp  (ReadDirectoryChangesW)
```

### 4.3 Config Section

```json
{
  "modules": {
    "filesystem": {
      "enabled": true,
      "watch_paths": ["/home", "/tmp", "/var/tmp"],
      "exclude_paths": ["/tmp/cache"],
      "events": ["create", "modify", "delete"]
    }
  }
}
```

### 4.4 Linux Implementation — `inotify`

```cpp
// Pseudocode
int fd = inotify_init1(IN_NONBLOCK);
for (const auto& path : watch_paths) {
    inotify_add_watch(fd, path.c_str(),
        IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
}

// Background thread
while (running) {
    select/poll on fd
    read events
    for each event:
        build Event
        event_bus.publish(event)
}
```

**Why `inotify` first:** Simpler than `fanotify`. Doesn't require root for basic watching. `fanotify` is for blocking decisions, which comes later in enforcement.

### 4.5 Windows Implementation — `ReadDirectoryChangesW`

Windows equivalent. Watch multiple directories using overlapped I/O or one thread per directory.

### 4.6 Success Criteria

1. Create a file in a watched directory → event logged
2. Modify the file → event logged
3. Delete the file → event logged
4. Works on both platforms
5. Config-driven — change watch paths, restart, new paths are watched

---

## 5. Milestone 8 — Scanner

**Goal:** Detect sensitive data in content.

### 5.1 What It Delivers

- `IScanner` interface
- `RegexScanner` — fast offline detection of common PII patterns
- `RemotePiiScanner` — calls the core PII API for deep analysis
- Unified `ScanResult` type
- Integration with the event bus (scanner subscribes, emits scan results)

### 5.2 New Files

```
src/
└── scanner/
    ├── scanner.h            (interface + ScanResult)
    ├── scanner_manager.{h,cpp}
    ├── regex_scanner.{h,cpp}
    ├── remote_pii_scanner.{h,cpp}
    └── patterns.h           (regex patterns)
```

### 5.3 The `ScanResult` Type

```cpp
struct ScanEntity {
    std::string type;      // "CREDIT_CARD", "EMAIL"
    int start;
    int end;
    double confidence;
    std::string text;      // the matched substring
};

struct ScanResult {
    bool sensitive = false;
    int risk_score = 0;    // 0-10
    std::vector<ScanEntity> entities;
    std::string source;    // "regex", "remote", ...
};
```

### 5.4 Regex Patterns

Built-in patterns for:

- `CREDIT_CARD` — Luhn-checked 13-19 digit numbers
- `SSN` — US Social Security numbers
- `EMAIL` — RFC 5322 simplified
- `PHONE_NUMBER` — various international formats
- `API_KEY` — common key patterns (AWS, GitHub, etc.)

**Why regex first:**

- Fast — microseconds per scan
- Offline — no network dependency
- Deterministic — same input always produces same output
- Simple to test

**Why not regex-only:** It catches structured PII, not names, addresses, or context-dependent data. That's what the remote PII API is for.

### 5.5 Strategy

```
scan(text):
  1. Run regex scanner → partial results
  2. If online and text is long or ambiguous → call remote PII API
  3. Merge results
  4. Return unified ScanResult
```

### 5.6 Success Criteria

1. A file containing a fake credit card number → detected
2. An email address → detected
3. Clean text → no false positives
4. Remote scanner correctly falls back when the core is unreachable

---

## 6. Milestone 9 — Policy Engine

**Goal:** Turn scan results into decisions.

### 6.1 What It Delivers

- `Policy` struct — a DLP rule
- `PolicyEngine` — evaluates events against policies
- `Decision` struct — the action to take
- Local policy cache (initially hardcoded, later SQLite)
- Integration with the event bus (subscribes to scan results, emits decisions)

### 6.2 New Files

```
src/
└── policy/
    ├── policy.h
    ├── policy_engine.{h,cpp}
    ├── policy_cache.{h,cpp}
    └── decision.h
```

### 6.3 The Policy Structure

```cpp
struct Policy {
    std::string id;
    std::string name;
    std::string entity_type;   // "CREDIT_CARD", "SSN", ...
    double threshold;          // min confidence 0.0-1.0
    std::string action;        // "block", "alert", "log", "allow"
    int priority;              // higher wins
    bool enabled;
    std::vector<std::string> applies_to;  // module names
};
```

### 6.4 The Decision Structure

```cpp
struct Decision {
    std::string action;        // "block", "alert", "log", "allow"
    int risk_score;
    std::string reason;
    std::string policy_id;     // which policy matched
    bool enforce_sync;         // block synchronously?
};
```

### 6.5 Evaluation Algorithm

```
decide(event, scan_result):
  candidates = policies where:
    - enabled
    - applies to event.type
    - entity_type matches a detected entity
    - confidence >= threshold

  if candidates.empty():
    return Decision{action: "allow"}

  sort candidates by priority descending
  pick the highest-priority match
  return Decision from that policy
```

### 6.6 Success Criteria

1. A scan result with a credit card number → `block` decision
2. A scan result with a low-confidence email → `log` decision
3. No policies matched → `allow` decision
4. Changing a policy's priority changes the outcome

---

## 7. Milestone 10 — Storage

**Goal:** Persist events, policies, and a queue of unsent events.

### 7.1 What It Delivers

- SQLite wrapper
- Migrations for schema versioning
- Event queue (persists across restarts)
- Policy cache table
- Audit log table
- Integration with the event bus (subscribes to all events, writes them)

### 7.2 New Files

```
src/
└── storage/
    ├── database.{h,cpp}
    ├── migrations.{h,cpp}
    ├── event_queue.{h,cpp}
    └── sqlite3_wrapper.{h,cpp}
```

### 7.3 Schema

```sql
-- Events that have occurred and their decisions
CREATE TABLE events (
    id TEXT PRIMARY KEY,
    timestamp INTEGER,
    type TEXT,
    user_id TEXT,
    process_name TEXT,
    pid INTEGER,
    data TEXT,
    sensitive INTEGER,
    risk_score INTEGER,
    action TEXT,
    uploaded INTEGER DEFAULT 0
);

-- Cached policies from the core
CREATE TABLE policies (
    id TEXT PRIMARY KEY,
    name TEXT,
    entity_type TEXT,
    threshold REAL,
    action TEXT,
    priority INTEGER,
    enabled INTEGER,
    applies_to TEXT,
    synced_at INTEGER
);

-- Sync state (policy version, last heartbeat, etc.)
CREATE TABLE state (
    key TEXT PRIMARY KEY,
    value TEXT
);
```

### 7.4 Why SQLite

- Embedded — no server process
- Transactional — ACID guarantees
- Fast — millions of inserts per second
- Portable — works identically on Linux and Windows
- Widely used — every serious C++ project uses it

### 7.5 Success Criteria

1. Every event is written to the database
2. Events survive an agent restart
3. The database schema is versioned (migrations run on startup)
4. Queries for recent events return in milliseconds

---

## 8. Milestone 11 — User-Mode Enforcement

**Goal:** Block operations when policy says so.

### 8.1 What It Delivers

- `IEnforcer` interface
- User-mode enforcement backends
- First target: clipboard paste (Linux)
- Second target: file write (both platforms)
- Integration with the policy engine

### 8.2 New Files

```
src/
└── enforcement/
    ├── enforcer.h
    ├── enforcer_registry.{h,cpp}
    └── backends/
        └── usermode/
            ├── usermode_enforcer.{h,cpp}
            ├── clipboard_enforcer_linux.cpp
            └── file_enforcer.cpp
```

### 8.3 The `IEnforcer` Interface

```cpp
class IEnforcer {
public:
    virtual ~IEnforcer() = default;
    virtual bool can_enforce(const std::string& event_type) = 0;
    virtual bool enforce(const Decision& decision, const Event& event) = 0;
};
```

**Why per-event-type:** Different events need different enforcement mechanisms. A clipboard event needs clipboard hooks; a file event needs file hooks.

### 8.4 First Target — Clipboard Block (Linux)

- Monitor X11 / Wayland clipboard selections
- When a paste is about to happen, scan the content
- If policy says "block", deny the paste

**Simpler alternative:** Since clipboard monitoring is complex, start with **file open blocking**. If a policy says a file should not be opened, refuse the syscall.

### 8.5 Success Criteria

1. A block decision prevents the operation
2. The user sees a clear message that the operation was blocked
3. The event is logged with the decision
4. The agent does not crash on enforcement failure

---

## 9. Milestone 12 — IPC + User Helper

**Goal:** Provide a user-facing UI (tray icon, notifications).

### 9.1 What It Delivers

- IPC server in the service
- IPC client in the helper
- Tray icon (Linux: system tray; Windows: notification area)
- Notifications for blocked events
- User can pause DLP (if policy allows)

### 9.2 New Files

```
helper/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── tray.{h,cpp}
│   ├── notifications.{h,cpp}
│   └── ipc_client.{h,cpp}
└── resources/
    └── icons/

src/
└── ipc/
    ├── ipc_server.{h,cpp}
    ├── messages.h
    └── ipc_linux.cpp / ipc_windows.cpp
```

### 9.3 IPC Protocol

Messages are length-prefixed JSON. Each message has:

```json
{
  "type": "notification",
  "title": "Blocked",
  "message": "Sensitive data prevented from being copied"
}
```

**Why JSON:** Already have a JSON library. Easy to debug. Extensible.

### 9.4 Success Criteria

1. Tray icon appears when the user logs in
2. User sees a notification when the agent blocks an event
3. User can pause/resume the agent (if policy allows)
4. The helper crashes don't affect the service

---

## 10. Milestone 13 — Transport

**Goal:** Connect to AxiomDLP Core.

### 10.1 What It Delivers

- HTTP client (libcurl)
- WebSocket client (Boost.Beast)
- Enrollment
- Heartbeat
- Event upload (batched)
- Policy sync
- Reconnect with exponential backoff
- Offline queue flush

### 10.2 New Files

```
src/
└── transport/
    ├── transport_manager.{h,cpp}
    ├── http_client.{h,cpp}
    ├── ws_client.{h,cpp}
    ├── heartbeat.{h,cpp}
    ├── reconnect.{h,cpp}
    └── protocol.h
```

### 10.3 Connection Model

- **Outbound only** — agent dials the core; core never dials the agent
- **HTTPS for bulk** — event upload, policy fetch
- **WebSocket for control** — policy updates, commands (push)
- **Reconnect** — exponential backoff with jitter, cap at 5 minutes
- **Offline** — events queue locally in SQLite; flush on reconnect

### 10.4 Success Criteria

1. Agent enrolls with a test core
2. Agent heartbeats every 30 seconds
3. Events are uploaded in batches
4. Policy changes on the core reach the agent within seconds
5. Connection loss is transparent — agent keeps working with cached policies

---

## 11. Milestone 14 — Kernel Enforcement

**Goal:** Move enforcement into the kernel for unbypassable blocking.

### 11.1 What It Delivers

- **Windows:** Minifilter driver for file operations
- **Linux:** eBPF LSM programs or `fanotify` with permission events
- Kernel-to-user communication channel
- User-mode policy evaluation remains
- Kernel-mode enforcement acts on the user-mode decisions

### 11.2 New Files

```
kernel/
├── windows/
│   └── minifilter/
│       ├── driver.{c,h}
│       ├── communication.{c,h}
│       └── install.ps1
└── linux/
    └── ebpf/
        ├── file_ops.bpf.c
        ├── loader.{c,h}
        └── build.sh
```

### 11.3 Architecture

```
User mode                        Kernel mode
─────────                        ───────────
                                
Event captured ──────────────►  Kernel driver
                                     │
                                     ├── Ask user-mode: block or allow?
                                     │
                                     ▼
                                User-mode policy engine
                                     │
                                     ├── Returns decision
                                     │
                                     ▼
                                Kernel driver enforces
                                     │
                                     └── Syscall returns EACCES
```

**Why kernel enforcement matters:** User-mode hooks can be bypassed. A malicious process can avoid them. Kernel enforcement is at the syscall layer — it cannot be bypassed by unprivileged code.

### 11.4 Success Criteria

1. Kernel driver loads and communicates with the user-mode agent
2. A blocked file operation returns `EACCES` (Linux) or `STATUS_ACCESS_DENIED` (Windows)
3. Attempting to bypass (e.g., via raw syscalls) still hits the driver
4. The driver does not crash the system under any input

---

## 12. Ordering Rationale

Why this order:

| Order | Milestone | Why Here |
|-------|-----------|----------|
| 5 | Module System | Every feature depends on this plugin architecture |
| 6 | Event Bus | Modules need to communicate without knowing each other |
| 7 | Filesystem | First real module — proves the module + event pattern |
| 8 | Scanner | Needed to detect sensitive content |
| 9 | Policy Engine | Turns scan results into decisions |
| 10 | Storage | Persist events before enforcement, so we have an audit trail |
| 11 | Enforcement | Start blocking — after we can scan, decide, and record |
| 12 | IPC + Helper | User-facing feedback |
| 13 | Transport | Connect to the core — final piece of the local pipeline |
| 14 | Kernel Enforcement | Deepest enforcement layer |

**The rule:** Never build a feature on top of an untested foundation. Each milestone is fully working before the next begins.

**Sequencing insight:** Enforcement comes *after* storage. Why? Because when the agent blocks something, we want to have a durable record of the block. If we enforced first, a crash could lose the record. Storage-first is safer.

---

## 13. Working Agreement

### 13.1 Per Milestone

1. **Design first** — the interface, the data types, the platform split
2. **Build the interface** — the header
3. **Build the example** — a dummy or minimal implementation
4. **Wire it in** — connect to the existing system
5. **Test on Linux** — foreground mode first
6. **Test as a service** — install, run, uninstall
7. **Cross-compile** — verify all four targets
8. **Commit** — descriptive message, one milestone per commit

### 13.2 Every Commit Must

- Build cleanly on all four targets
- Run without crashes in foreground mode
- Not break any previously working feature
- Have a message that describes the change

### 13.3 Every New Subsystem Must

- Follow the three-file pattern: header, implementation, CMake wiring
- Have an interface in `core/` if it's cross-platform
- Have platform implementations in `platform/<os>/` if it's OS-specific
- Be fail-safe: errors are logged, not fatal
- Be testable in isolation

### 13.4 Every New Module Must

- Implement `IModule`
- Receive `ServiceContext`
- Publish or subscribe to events via the bus
- Report health
- Be registered in one place (Application)
- Not modify the core or other modules

### 13.5 Current Command-Line Interface

| Flag | Effect |
|------|--------|
| (none) | Run as a service |
| `--foreground` | Run in the terminal (development) |
| `--install` | Install the service |
| `--uninstall` | Remove the service |
| `--config <path>` | Use a specific config file |

Future flags (as milestones are added):

| Flag | Milestone |
|------|-----------|
| `--version` | Anytime |
| `--log-level <level>` | Anytime |
| `--no-modules` | 5 |
| `--module <name>` | 5 |

### 13.6 Current Build Commands

| Task | Command |
|------|---------|
| Build all | `./scripts/build-all.sh` |
| Build one | `cmake --build --preset <name>` |
| Clean build | `rm -rf build && ./scripts/build-all.sh` |
| Clean cache | `rm -rf .deps` (forces re-download) |

### 13.7 Verification Checklist Per Milestone

- [ ] Builds on Linux x86_64
- [ ] Builds on Linux i686
- [ ] Builds on Windows x86_64
- [ ] Builds on Windows i686
- [ ] Runs in Linux foreground mode
- [ ] Runs in Windows foreground mode
- [ ] Installs as a Linux systemd service
- [ ] Installs as a Windows SCM service
- [ ] Uninstalls cleanly (no orphan processes)
- [ ] Logs new behavior in journald / log file
- [ ] Committed with a clear message

---

## Current State Summary

| What | Status |
|------|--------|
| Cross-compilation | ✅ Complete |
| Async logging | ✅ Complete |
| Config loading | ✅ Complete |
| Service lifecycle | ✅ Complete & verified |
| Platform abstraction | ✅ Complete |
| Module system | ⬜ Next |
| Event bus | ⬜ Planned |
| DLP features | ⬜ Planned |

**Next action:** Start Milestone 5 — Module System.

The design is in this document. The implementation begins with `src/core/module.h`, followed by `module_manager.{h,cpp}`, `service_context.{h,cpp}`, and the example module.