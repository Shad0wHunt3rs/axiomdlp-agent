# AxiomDLP Endpoint Agent — DONE.md

> **Status document — what has actually been implemented and verified.**
>
> This document is the truthful mirror of the codebase. Every claim here can be verified by reading the source. It supersedes any status section in the other docs (README.md, next.md, architecture.md) where those are stale.

**Last updated:** After Milestone 7 completion + full bug-fix pass on 2026-09-24.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [What Exists vs What Was Planned](#2-what-exists-vs-what-was-planned)
3. [Repository Layout (Actual)](#3-repository-layout-actual)
4. [Build System](#4-build-system)
5. [Cross-Compilation Targets](#5-cross-compilation-targets)
6. [The Core Framework](#6-the-core-framework)
7. [The Module System](#7-the-module-system)
8. [The Event Bus](#8-the-event-bus)
9. [The Filesystem Module](#9-the-filesystem-module)
10. [The Event Logger Module](#10-the-event-logger-module)
11. [The Example Module](#11-the-example-module)
12. [Platform Abstraction Layer](#12-platform-abstraction-layer)
13. [Service Lifecycle (Linux + Windows)](#13-service-lifecycle-linux--windows)
14. [Configuration System](#14-configuration-system)
15. [Logging Subsystem](#15-logging-subsystem)
16. [Bug Fixes Applied](#16-bug-fixes-applied)
17. [Runtime Evidence — What the Logs Prove](#17-runtime-evidence--what-the-logs-prove)
18. [Known Limitations](#18-known-limitations)
19. [What Does Not Exist Yet](#19-what-does-not-exist-yet)
20. [Commands Reference](#20-commands-reference)
21. [Verification Checklist](#21-verification-checklist)
22. [Next Milestones](#22-next-milestones)

---

## 1. Executive Summary

The AxiomDLP Endpoint Agent is a **working, cross-platform background service** with a complete foundation and its first real DLP data source running. It is no longer a bare skeleton.

**What runs today:**

- Builds four separate binaries from a single Linux host (Linux 32/64, Windows 32/64)
- Runs as a privileged service on Linux (systemd) and Windows (SCM)
- Reads JSON config with fail-safe defaults
- Writes async structured logs to console + rotating file
- Hosts three modules that start, stop, and shut down cleanly
- Captures real filesystem activity, debounces it, publishes it on the event bus
- Logs every event for observability
- Excludes configured paths and its own log directory

**What does not exist yet:**

- Scanner (regex / remote PII)
- Policy engine
- Storage (SQLite)
- Enforcement
- Transport to AxiomDLP Core
- Kernel drivers

The agent is a **monitoring pipeline without a decision-maker**. It sees everything, records it, and does nothing about it. That is exactly the right shape at this milestone.

---

## 2. What Exists vs What Was Planned

The roadmap in `next.md` laid out milestones in a specific order. The actual code has moved faster than the doc reflects. The table below is the honest reconciliation.

| Milestone | Description | Planned | Actual |
|-----------|-------------|---------|--------|
| 1 | Project skeleton | ✅ | ✅ Done |
| 2 | Async logging with spdlog | ✅ | ✅ Done |
| 3 | Config loading | ✅ | ✅ Done |
| 4 | Cross-platform service lifecycle | ✅ | ✅ Done |
| 5 | Module system (plugin architecture) | ⬜ | ✅ **Done** |
| 6 | Event bus (pub/sub) | ⬜ | ✅ **Done** |
| 7 | Filesystem module | ⬜ | ✅ **Done** (efsw-backed) |
| — | Event logger module (bonus) | — | ✅ Done |
| — | Full bug-fix pass (feedback loop, exclusions, debounce priority, config path, install flow) | — | ✅ Done |
| 8 | Scanner | ⬜ | **Next** |
| 9 | Policy engine | ⬜ | Planned |
| 10 | Storage (SQLite) | ⬜ | Planned |
| 11 | User-mode enforcement | ⬜ | Planned |
| 12 | IPC + tray helper | ⬜ | Planned |
| 13 | Transport (HTTP + WS) | ⬜ | Planned |
| 14 | Kernel enforcement | ⬜ | Planned |

**The documentation lag:** `README.md` and `next.md` still say "Milestone 5 is next." This document is authoritative on what exists. Update those files when convenient, but treat this one as the source of truth.

---

## 3. Repository Layout (Actual)

```
axiomdlp-agent/
├── CMakeLists.txt                    Root build recipe
├── CMakePresets.json                 8 named build configurations
├── compile_commands.json             Merged clangd DB (gitignored)
├── .clangd                           clangd config
├── .vscode/settings.json             clangd + query-driver
├── .gitignore
├── LICENSE                           Apache 2.0
├── README.md                         Stale (see DONE.md)
├── next.md                           Stale roadmap
├── done.md                           This file
│
├── .deps/                            Cached FetchContent sources (gitignored)
│   ├── spdlog-src/
│   ├── nlohmann_json-src/
│   └── efsw-src/
│
├── cmake/
│   └── toolchains/
│       ├── linux-x86_64.cmake
│       ├── linux-i686.cmake
│       ├── windows-x86_64.cmake
│       └── windows-i686.cmake
│
├── config/
│   ├── agent.default.json            Linux defaults
│   ├── agent.default_win.json        Windows defaults
│   └── agent.schema.json             JSON Schema
│
├── docs/                             Reference architecture
│   ├── architecture.md
│   ├── event_schema.md
│   ├── file_structure.md
│   ├── ipc_protocol.md
│   ├── modules.md
│   └── policy_format.md
│
├── logs/                             Runtime log output (gitignored)
│   └── agent.log
│
├── packaging/
│   ├── linux/systemd/                Reference unit file
│   └── windows/                      Reference install script
│
├── scripts/
│   ├── build-all.sh                  Build all four targets
│   └── merge-compile-commands.sh     Merge 6 DBs into one for clangd
│
└── src/
    ├── main.cpp                      5-line entry point
    │
    ├── core/
    │   ├── application.{h,cpp}       Orchestration + arg parsing + config resolution
    │   ├── config.{h,cpp}            JSON load + validate + defaults
    │   ├── event.{h,cpp}             Event struct + factory
    │   ├── event_bus.{h,cpp}         Thread-safe pub/sub
    │   ├── file_watcher.h            IFileWatcher interface
    │   ├── health.h                  HealthStatus types
    │   ├── logger.{h,cpp}            Async spdlog setup
    │   ├── module_context.h          Shared module dependency container
    │   ├── module_manager.{h,cpp}    Lifecycle orchestrator
    │   ├── paths.h                   Platform path API (interface)
    │   └── service.h                 IService interface
    │
    ├── interfaces/
    │   └── i_module.h                The 7-method module contract
    │
    ├── modules/
    │   ├── module_base.{h,cpp}       Convenience base for modules
    │   ├── example/
    │   │   ├── example_module.{h,cpp}
    │   ├── event_logger/
    │   │   ├── event_logger_module.{h,cpp}
    │   └── filesystem/
    │       ├── filesystem_module.{h,cpp}
    │
    └── platform/
        ├── common/
        │   └── efsw_file_watcher.cpp IFileWatcher impl via efsw
        ├── linux/
        │   ├── paths_linux.cpp       /proc/self/exe, /etc, /var/log, /var/lib
        │   └── service_linux.cpp     systemd + sd_notify + install/uninstall
        └── windows/
            ├── paths_windows.cpp     GetModuleFileNameA, SHGetKnownFolderPath
            └── service_win.cpp       SCM + ServiceMain + install/uninstall
```

**32 source files** across 11 directories. No orphaned or placeholder files.

---

## 4. Build System

### 4.1 CMake

- **CMake ≥ 3.24** (uses `CMakePresets.json` v6 features)
- **Ninja** as the generator
- **C++20** with extensions disabled
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd
- `CMAKE_POSITION_INDEPENDENT_CODE ON`

### 4.2 Dependencies (all fetched or system)

| Library | How It's Obtained | Purpose |
|---------|-------------------|---------|
| **spdlog** v1.14.1 | FetchContent + `.deps/` cache | Async logging |
| **nlohmann/json** v3.11.3 | System on Linux; FetchContent on Windows | Config parsing |
| **efsw** v1.7.2 | FetchContent + `.deps/` cache | File watching (wraps inotify/Win32) |
| **libsystemd** | `pkg-config` (Linux only, optional) | `sd_notify` |

`spdlog` is compiled (`SPDLOG_COMPILED_LIB`), not header-only. This gives faster builds and smaller objects across the four targets.

### 4.3 Source Caching

`.deps/` holds git-checked-out sources so deleting `build/` never re-downloads. The root `CMakeLists.txt` sets `FETCHCONTENT_SOURCE_DIR_*` variables when the cached directories exist.

### 4.4 Compile Options

- **Linux:** `-fstack-protector-strong -fPIE` compile, `-pie -Wl,-z,relro,-z,now` link
- **Windows:** `-static -static-libgcc -static-libstdc++` (no DLL dependencies)
- **Warnings:** `-Wall -Wextra -Wpedantic` on both

### 4.5 Windows Runtime

The Windows binaries are **fully static**. They do not require:
- MSVC redistributable
- `libstdc++-6.dll`
- `libgcc_s_seh-1.dll`

They run on **Windows 7 SP1 or later**, x86 and x64.

---

## 5. Cross-Compilation Targets

Eight presets, four targets, two build types each:

| Preset | Output | Compiler |
|--------|--------|----------|
| `linux-x86_64-debug` / `-release` | `build/linux-x86_64-*/axiomdlp-agent` | `/usr/bin/g++` |
| `linux-i686-debug` / `-release` | `build/linux-i686-*/axiomdlp-agent` | `/usr/bin/g++ -m32` |
| `windows-x86_64-debug` / `-release` | `build/windows-x86_64-*/axiomdlp-agent.exe` | `x86_64-w64-mingw32-g++` |
| `windows-i686-debug` / `-release` | `build/windows-i686-*/axiomdlp-agent.exe` | `i686-w64-mingw32-g++` |

**Toolchain isolation:** The Windows toolchains set:

```cmake
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

This prevents a Linux `.so` from ever linking into a Windows `.exe`. Verified in `compile_commands.json`: Windows targets reference MinGW headers, Linux targets reference `/usr/include`.

**Prerequisites (Kali/Debian/Ubuntu):**

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y \
    build-essential gcc-multilib g++-multilib \
    mingw-w64 ninja-build git file clangd \
    libsystemd-dev libsystemd-dev:i386 \
    libspdlog-dev nlohmann-json3-dev
```

**Build all four:**

```bash
./scripts/build-all.sh
```

First run ~2 minutes. Subsequent runs ~1 minute.

---

## 6. The Core Framework

### 6.1 `Application` — `src/core/application.{h,cpp}`

The top-level orchestrator. Owns everything with a non-trivial lifetime.

**Responsibilities:**

1. Parse command-line arguments (`--install`, `--uninstall`, `--foreground`, `--config <path>`)
2. Resolve the config path via a fallback chain (see §14.1)
3. Load `Config` (fail-safe; warnings collected, never throws)
4. Initialize the `Logger`
5. Create the `EventBus`
6. Create the `ModuleContext` (references the `Config` and `EventBus`)
7. Create the `ModuleManager`
8. Register all modules in order
9. Call `initialize_all()` on the manager
10. For `--install`/`--uninstall`: short-circuit before full init, use a minimal `install.log`
11. For normal run: create the platform `IService`, wire in the manager, run

**Shutdown (RAII):** `Application::~Application()` calls `shutdown()` which calls `Logger::shutdown()`. This flushes any pending async log messages before the process exits.

**Short-circuit logic (new):**

```cpp
int Application::execute() {
    if (opts_.install || opts_.uninstall) {
        Logger::init("logs", "install.log", "info");
        // run install/uninstall, then return
    }
    if (!init()) return 1;
    service_ = make_service(...);
    service_->set_module_manager(module_manager_.get());
    return service_->run();
}
```

Install/uninstall no longer create the module tree or the event bus.

### 6.2 `Config` — `src/core/config.{h,cpp}`

See §14 for full detail. Summary:

- Nested structs mirror the JSON structure
- Every field has a safe default
- Loading never throws; problems are collected as warnings
- Missing file → warning + defaults
- Invalid JSON → warning + defaults
- Wrong type per field → warning + default for that field only
- Clamping applied to integer fields (negative `debounce_ms` → 300)

### 6.3 `Logger` — `src/core/logger.{h,cpp}`

See §15 for full detail. Summary:

- spdlog async logger
- 8192-slot queue, 1 background worker
- Console sink (colored) + rotating file sink (5 MB × 3)
- `overrun_oldest` policy: never blocks the caller
- Format: `[timestamp] [level] [thread-id] message`
- Flush on `warn` and above
- Registered as the global default logger

### 6.4 `Event` — `src/core/event.{h,cpp}`

```cpp
struct Event {
    std::string id;                    // "1790235151064336632-1" (nanos-counter)
    std::string type;                  // "file_write", ...
    std::chrono::system_clock::time_point timestamp;
    std::string user_id;               // reserved, not populated yet
    std::string process_name;          // reserved, not populated yet
    int pid = 0;                       // reserved, not populated yet
    std::map<std::string, std::string> metadata;
    std::optional<std::string> content;   // reserved for Scanner
    static Event create(const std::string& type);
};
```

ID generation is monotonic: nanoseconds since epoch + atomic counter. Two events created in the same nanosecond still get distinct IDs.

Canonical type strings in `event_types::`: `kFileAccess`, `kFileWrite`, `kFileDelete`, `kUsbInsert`, `kUsbRemove`, `kClipboardCopy`, `kPrintJob`, `kProcessSpawn`, `kScanResult`, `kDecision`.

### 6.5 `HealthStatus` — `src/core/health.h`

```cpp
enum class HealthLevel { Unknown, OK, Degraded, Failed };

struct HealthStatus {
    HealthLevel level;
    std::string message;
    static HealthStatus ok(msg);
    static HealthStatus degraded(msg);
    static HealthStatus failed(msg);
    static HealthStatus unknown(msg);   // NEW
    bool is_healthy() const;
};
```

Four levels, not two. `Degraded` covers "working but slow / partially functional." `Unknown` covers "not yet checked" and "not initialized."

### 6.6 `paths` — `src/core/paths.h` + platform impls

Cross-platform path API. One header, two implementations selected at build time.

| Function | Linux | Windows |
|----------|-------|---------|
| `executable_path()` | `readlink("/proc/self/exe")` | `GetModuleFileNameA` |
| `executable_dir()` | `std::filesystem::path` | same |
| `default_config_path()` | `/etc/axiomdlp/agent.json` | `%ProgramData%\AxiomDLP\agent.json` |
| `default_log_dir()` | `/var/log/axiomdlp` | `%ProgramData%\AxiomDLP\logs` |
| `default_data_dir()` | `/var/lib/axiomdlp` | `%ProgramData%\AxiomDLP` |

Windows uses `SHGetKnownFolderPath(FOLDERID_ProgramData)` and converts wide strings to UTF-8 via `WideCharToMultiByte`.

### 6.7 `IService` — `src/core/service.h`

Three-method interface:

```cpp
class IService {
public:
    virtual int run() = 0;                       // blocks until stopped
    virtual void request_stop() = 0;
    virtual void set_module_manager(ModuleManager* mgr) = 0;
};
```

Plus free functions: `make_service()`, `install_service()`, `uninstall_service()`, `default_service_name()`.

`request_stop()` is implemented but not currently called by any path (signals go directly to a global atomic). It exists so a future IPC command handler can stop the service without raising a signal.

---

## 7. The Module System

### 7.1 `IModule` — `src/interfaces/i_module.h`

The contract every feature implements. Seven methods:

| Method | Returns | Called |
|--------|---------|--------|
| `name()` | `std::string` | identity |
| `version()` | `std::string` | identity |
| `initialize(ModuleContext&)` | `bool` | once, before start; may fail |
| `start()` | `bool` | after all init; may fail |
| `stop()` | `void` | shutdown; idempotent |
| `shutdown()` | `void` | release resources |
| `health()` | `HealthStatus` | on demand |

**Why `bool` on initialize/start:** they can fail (missing permissions, no watch paths). A failure is logged and does not stop other modules.

**Why `void` on stop/shutdown:** best-effort. Errors are logged inside, never propagated.

### 7.2 `ModuleBase` — `src/modules/module_base.{h,cpp}`

Convenience base class. Provides:

- Default `version()` returning `"1.0.0"`
- Default `initialize()` that stores `ctx_ = &context` and returns `true`
- Default no-op `start()`, `stop()`, `shutdown()`
- Default `health()` returning `HealthStatus::ok()`
- Protected member `ModuleContext* ctx_ = nullptr`

Modules override only what they need. `ExampleModule` overrides everything; `EventLoggerModule` overrides only `start()` and `stop()`.

### 7.3 `ModuleContext` — `src/core/module_context.h`

```cpp
struct ModuleContext {
    const Config& config;
    EventBus&     event_bus;
    ModuleContext(const Config&, EventBus&);
};
```

Struct-of-references, not pointers. Non-null by construction. Grows over time (Storage, PolicyEngine, Scanner, Transport will be added in later milestones).

Modules receive the whole context; they only use what they need.

### 7.4 `ModuleManager` — `src/core/module_manager.{h,cpp}`

Owns `std::vector<std::unique_ptr<IModule>>`. Drives the lifecycle.

| Method | Behavior |
|--------|----------|
| `register_module(unique_ptr)` | Takes ownership; logs name + version |
| `initialize_all(ctx)` | Forward order; returns `false` if any failed |
| `start_all()` | Forward order; returns `false` if any failed |
| `stop_all()` | **Reverse order**; idempotent |
| `shutdown_all()` | **Reverse order**; clears the vector |
| `count()` | Number of registered modules |
| `health()` | Aggregates all modules into a single `HealthStatus` |

**Why reverse on stop/shutdown:** LIFO. `EventLogger` subscribes to events; `Filesystem` publishes them. If `Filesystem` stops first, `EventLogger` has nothing to log while shutting down. Reverse order guarantees the consumer outlives the producer.

**Fail-safe:** One module failing does not prevent the manager from calling the others. `initialize_all` and `start_all` return `false` only to *report*, not to *abort*.

**Health aggregation:** Counts OK / Degraded / Failed / Unknown, produces a summary string like `"2 OK, 1 failed, 0 unknown"`, and returns the worst level found.

### 7.5 Adding a New Module

1. Create the class, inherit from `ModuleBase`
2. Register it in `Application::init()` — one line
3. Add the `.cpp` to `CMakeLists.txt`

The service loop never changes. This was validated three times (Example → EventLogger → Filesystem) with zero changes to `core/`.

---

## 8. The Event Bus

### 8.1 `EventBus` — `src/core/event_bus.{h,cpp}`

In-process synchronous publish/subscribe.

```cpp
class EventBus {
public:
    using EventHandler = std::function<void(const EventPtr&)>;
    SubscriptionId subscribe(const std::string& type, EventHandler);
    SubscriptionId subscribe_all(EventHandler);
    void unsubscribe(SubscriptionId);
    void publish(EventPtr);
    std::size_t subscriber_count() const;
};
```

`EventPtr = std::shared_ptr<Event>` — one event instance is shared with all subscribers, no copies.

`SubscriptionId` is a `uint64_t`, monotonically increasing.

### 8.2 Threading Model

- `subscribe`, `unsubscribe`, `publish`, `subscriber_count` are **all thread-safe**
- A single `std::mutex` guards the subscription list
- `publish` copies matching handlers **under the lock**, then releases the lock, then invokes them
- This means a handler can safely call `subscribe`/`unsubscribe` during its own invocation — no deadlock
- Each handler is called inside a `try/catch`; one throwing subscriber does not stop the others

### 8.3 Subscriber Matching

- `subscribe(type, handler)` — matches when `event->type == type`
- `subscribe_all(handler)` — matches every event (stored as empty `type`)
- Both are stored in the same vector; the match check is `sub.type.empty() || sub.type == event->type`

### 8.4 Synchronous vs Asynchronous

The bus is **synchronous**: `publish` calls each handler on the publishing thread before returning. This was a deliberate choice:

- Simple, predictable, no queue to manage
- A slow subscriber stalls that one publisher, not the whole agent
- Given that the current publisher is a single watcher thread, the cost is bounded

If profiling later shows a bottleneck, the switch to async is contained inside `publish`.

### 8.5 Standard Event Types

Declared in `src/core/event.h` under `namespace event_types`:

```
kFileAccess    = "file_access"
kFileWrite     = "file_write"
kFileDelete    = "file_delete"
kUsbInsert     = "usb_insert"
kUsbRemove     = "usb_remove"
kClipboardCopy = "clipboard_copy"
kPrintJob      = "print_job"
kProcessSpawn  = "process_spawn"
kScanResult    = "scan_result"
kDecision      = "decision"
```

Strings, not enums. New types can be added without recompiling subscribers. The compiler catches typos in publishers via the `inline constexpr` declarations.

---

## 9. The Filesystem Module

### 9.1 `FilesystemModule` — `src/modules/filesystem/filesystem_module.{h,cpp}`

The first real DLP data source. Watches directories, debounces kernel events, publishes `Event`s onto the bus.

### 9.2 `IFileWatcher` — `src/core/file_watcher.h`

Platform-agnostic interface.

```cpp
struct FileEvent {
    std::string path;
    std::string event_type;    // "created" / "modified" / "deleted" / "moved"
    std::string old_path;      // for "moved"
    bool is_directory = false;
    std::chrono::system_clock::time_point timestamp;
};

class IFileWatcher {
public:
    using Callback = std::function<void(const FileEvent&)>;
    virtual bool start(const std::vector<std::string>& paths, Callback) = 0;
    virtual void stop() = 0;
};
```

Factory: `std::unique_ptr<IFileWatcher> make_file_watcher();`

### 9.3 `EfswFileWatcher` — `src/platform/common/efsw_file_watcher.cpp`

The **only** file in the entire project that includes `<efsw/efsw.hpp>`. Everything else uses `IFileWatcher`.

efsw internally chooses:
- **Linux:** inotify
- **Windows:** `ReadDirectoryChangesW` via I/O Completion Ports

**Partial-failure handling (implemented):** When watching a list of paths, a failure on one path logs a warning and continues. Only if *every* path fails does `start()` return `false`.

From the runtime log:

```
EfswFileWatcher: cannot watch '/' (permission denied, missing, or unsupported); skipping
EfswFileWatcher: cannot watch '/data' (permission denied, missing, or unsupported); skipping
EfswFileWatcher: cannot watch '/workspace' (permission denied, missing, or unsupported); skipping
EfswFileWatcher: watching '/root' (id=1)
EfswFileWatcher: watching '/home' (id=162)
...
EfswFileWatcher: started, watching 11 path(s) (3 skipped)
```

The watcher started with 11 of 14 paths. This is the desired behavior: **a bad path must never bring down the watcher.**

**Callback exception isolation:** The efsw callback is wrapped in try/catch. A throwing module callback cannot escape into efsw's thread.

### 9.4 Trailing-Edge Debouncing

Raw kernel events are noisy. A single `echo "x" > file` produces 3–5 kernel events (open, write, close, metadata). Publishing each one would:
- Scan the same file multiple times
- Publish partial content
- Flood the bus

**Solution:** trailing-edge debouncing keyed by path.

1. When a `FileEvent` arrives, look up its path in `pending_`
2. If new, insert with a deadline of `now + debounce_window`
3. If already present, **only overwrite if the new event has equal or higher priority** (see §9.5), and **always reset the deadline**
4. A background flusher thread wakes every 50 ms, scans `pending_`, and publishes any entry whose deadline has passed

**Runtime evidence from the log (before vs after debounce):**

| Run | Raw writes to `/tmp/dlp-test/file1.txt` | Published events |
|-----|----------------------------------------|------------------|
| 12:32 (no debounce) | 3 writes | 3 events |
| 12:34 (no debounce) | 6 writes | 6 events |
| 12:45 (debounce=300ms) | 3 writes | **1 event** |

### 9.5 Priority-Based Debounce Merge (Bug Fix #3)

If a file is created, modified, then deleted within the window, the naive merge (`event = new_event`) would emit only `deleted`. That is catastrophic for DLP: you cannot scan a deleted file's content.

**Priority table:**

| Event type | Priority |
|-----------|----------|
| `modified` | 4 |
| `created` | 3 |
| `moved` | 2 |
| `deleted` | 1 |

The merge only overwrites when `new_priority >= existing_priority`. A `deleted` event cannot clobber a `modified` event. But the deadline *is* always extended, so we don't emit a still-being-written file.

### 9.6 Exclusion Filtering (Bug Fix #2)

`on_file_event` calls `is_excluded(path)` before touching the pending map. If the path matches any pattern, the event is dropped.

**Pattern matching:** `path_matches_pattern` tries two forms:

1. **Glob match:** supports `*` (matches any sequence, including `/`) and `?` (matches any single character). Not full POSIX glob (no character classes), but covers every pattern in the shipped configs.
2. **Directory prefix:** if the pattern has no trailing wildcard, treat it as a directory and match its entire subtree. So `/var/tmp` matches `/var/tmp/anything/inside`.

Examples that work:
- `*/node_modules/*` matches `/home/user/proj/node_modules/foo/bar.js`
- `*/.cache/*` matches `/home/user/.cache/chrome/Cache/...`
- `/var/tmp` matches `/var/tmp/systemd-private-abc123/tmp`

### 9.7 Implicit Log-Directory Exclusion (Bug Fix #1)

Before the fix, `EventLogger` writes to `logs/agent.log`, which the filesystem module watched, so every log line became a new `file_write` event → new log line → infinite loop at ~3 Hz.

The fix: `FilesystemModule::build_excludes()` always appends `<absolute(logging.dir)>/*` to the exclusion list. The path is lexical-absolute'd so it works regardless of the CWD the agent started in.

**Verification:** In the runtime logs the string `agent.log` appears zero times as an event path after the fix. Before the fix, it appeared 8+ times in 15 minutes.

### 9.8 Directory Event Skipping

efsw fires `modified` events on directories when their list of entries changes — not when file content changes. These are pure noise for DLP. `on_file_event` checks `fe.is_directory` and returns early.

### 9.9 Null-Guard in `health()`

If a module fails `initialize()`, `ctx_` is never assigned. `ModuleManager::health()` iterates all modules, so a failed module's `health()` would dereference null. Fixed:

```cpp
HealthStatus FilesystemModule::health() const {
    if (ctx_ == nullptr) return HealthStatus::unknown("not initialized");
    ...
}
```

### 9.10 Bus Event Mapping

File-event types map to bus-event types:

| efsw action | bus event type |
|-------------|----------------|
| `created` | `file_write` |
| `modified` | `file_write` |
| `deleted` | `file_delete` |
| `moved` | `file_access` |

The published `Event` carries:

- `metadata["path"]` — absolute path
- `metadata["source"]` = `"filesystem"`
- `metadata["fs_action"]` — original efsw action string
- `metadata["old_path"]` — only when `moved`

---

## 10. The Event Logger Module

### 10.1 Purpose

`EventLoggerModule` subscribes to **every** event on the bus and logs it. Its only purpose is observability.

- It is a pure consumer — it never publishes
- It proves the module + event bus pattern works end-to-end
- It gives a visible trace of everything flowing through the pipeline
- It will be the first module to be removed or moved to debug-only when a real consumer (Scanner) arrives

### 10.2 Implementation

```cpp
bool EventLoggerModule::start() {
    subscription_id_ = ctx_->event_bus.subscribe_all([](const EventPtr& e) {
        SPDLOG_DEBUG("[event] type={} path={}",
                     e->type,
                     e->metadata.count("path") ? e->metadata.at("path") : "-");
    });
    return true;
}

void EventLoggerModule::stop() {
    if (ctx_ && subscription_id_ != 0) {
        ctx_->event_bus.unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
}
```

### 10.3 Log Level (Bug Fix #6)

Originally logged at `INFO`. Combined with the feedback loop, this was the primary amplification path. Downgraded to `DEBUG` so it is only visible when the operator explicitly sets `logging.level = "debug"`.

### 10.4 Registration Order

Registered **last** in `Application::init()`. Because `ModuleManager::stop_all()` runs in reverse, this means `EventLogger` stops **first** — before `Filesystem` stops publishing. This ordering is intentional: during shutdown, we want to keep logging until producers go quiet.

---

## 11. The Example Module

### 11.1 Purpose

A minimal proof of the module system. It:

- Logs `"example module: initializing"` on `initialize`
- Subscribes to its own `example_ping` event on `start`
- Publishes one `example_ping` event
- Logs the received ping (proving the round trip)
- Logs on `stop` and `shutdown`

It is redundant now that `FilesystemModule` proves the pattern, but it is kept as the smallest possible module for onboarding and testing.

### 11.2 Runtime Evidence

```
ModuleManager: registering module 'example' v1.0.0
ModuleManager: initializing 'example'
example module: initializing
ModuleManager: starting 'example'
example module: starting
example module: subscribed (id=1)
example module: received ping (id=1790235020426945319-0)
```

The ping travels: subscriber → event bus → publisher → same subscriber's callback. Round trip verified.

---

## 12. Platform Abstraction Layer

### 12.1 Design Rule

`src/core/` and `src/modules/` **never** include `<windows.h>`, `<sys/inotify.h>`, `<systemd/sd-daemon.h>`, or any other OS header. Only `src/platform/<os>/` does.

### 12.2 Interfaces in `core/`

| Header | Contract |
|--------|----------|
| `service.h` | `IService` — run, stop, wire modules |
| `paths.h` | Executable, config, log, data directories |
| `file_watcher.h` | `IFileWatcher` — start watching, stop watching |

### 12.3 Implementations in `platform/`

| File | Platform | Provides |
|------|----------|----------|
| `common/efsw_file_watcher.cpp` | both | `IFileWatcher` via efsw |
| `linux/paths_linux.cpp` | Linux | `/proc/self/exe`, `/etc`, `/var/log`, `/var/lib` |
| `linux/service_linux.cpp` | Linux | systemd, `sd_notify`, install/uninstall |
| `windows/paths_windows.cpp` | Windows | `GetModuleFileNameA`, `SHGetKnownFolderPath` |
| `windows/service_win.cpp` | Windows | SCM, `ServiceMain`, install/uninstall |

### 12.4 CMake Selection

```cmake
if(WIN32)
    set(AXIOM_PLATFORM_SOURCES
        src/platform/windows/paths_windows.cpp
        src/platform/windows/service_win.cpp)
elseif(UNIX AND NOT APPLE)
    set(AXIOM_PLATFORM_SOURCES
        src/platform/linux/paths_linux.cpp
        src/platform/linux/service_linux.cpp)
endif()
```

One file per target. No runtime dispatch. No `#ifdef _WIN32` in application code.

### 12.5 Optional Features

`libsystemd` is detected at configure time:

```cmake
pkg_check_modules(SYSTEMD QUIET libsystemd)
if(SYSTEMD_FOUND)
    target_compile_definitions(... PRIVATE HAVE_SYSTEMD)
endif()
```

And guarded in C++:

```cpp
#ifdef HAVE_SYSTEMD
    sd_notify(0, "READY=1");
#endif
```

The agent builds and runs identically on a system without libsystemd — it just skips the readiness notification.

---

## 13. Service Lifecycle (Linux + Windows)

### 13.1 Linux (`src/platform/linux/service_linux.cpp`)

**Run mode:**
1. Install `SIGTERM` and `SIGINT` handlers → set `g_stop` atomic
2. Ignore `SIGHUP`
3. `module_manager_->start_all()`
4. `sd_notify(0, "READY=1")` if compiled with `HAVE_SYSTEMD`
5. Main loop: `sleep(200ms)` until `g_stop` is set
6. `module_manager_->stop_all()` then `shutdown_all()`
7. `sd_notify(0, "STOPPING=1")`

**Install (`--install`):**
1. Validate the service name (see §16 fix #17)
2. Write `/etc/systemd/system/<name>.service` with `Type=notify`
3. `systemctl daemon-reload`
4. `systemctl enable --now <name>`

The unit file:

```ini
[Unit]
Description=AxiomDLP Endpoint Agent
After=network.target

[Service]
Type=notify
ExecStart=/path/to/axiomdlp-agent
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

**Uninstall (`--uninstall`):**
1. `systemctl stop <name>`
2. `systemctl disable <name>`
3. Remove the unit file
4. `systemctl daemon-reload`

**Runtime evidence (from logs):**

```
installed unit file: /etc/systemd/system/axiomdlp-agent.service
reloaded systemd configuration
enabled and started service axiomdlp-agent
...
stopping service axiomdlp-agent
  stop: ok
disabling service axiomdlp-agent
  disable: ok
removed unit file: /etc/systemd/system/axiomdlp-agent.service
reloaded systemd configuration
uninstalled service axiomdlp-agent
```

Both directions verified end-to-end.

**No shell:** `run_program` uses `fork` + `execvp`, not `system()`. No injection surface.

### 13.2 Windows (`src/platform/windows/service_win.cpp`)

**Run mode:**
1. `StartServiceCtrlDispatcherA` — if it returns `ERROR_FAILED_SERVICE_CONTROLLER_CONNECT`, fall back to foreground mode
2. `RegisterServiceCtrlHandlerExA` with a control handler
3. Report `SERVICE_START_PENDING`
4. `module_manager_->start_all()`
5. Report `SERVICE_RUNNING`
6. Main loop: `sleep(200ms)` until `g_stop` is set
7. `module_manager_->stop_all()` then `shutdown_all()`
8. Report `SERVICE_STOPPED`

**Control handler:** accepts `SERVICE_CONTROL_STOP` and `SERVICE_CONTROL_SHUTDOWN`, sets `g_stop`, reports `SERVICE_STOP_PENDING`.

**Foreground fallback:** if launched from a terminal, the SCM dispatch fails and the agent runs the same loop in the console. This is why `--foreground` is not required on Windows.

**Install/uninstall:** uses `OpenSCManagerA`, `CreateServiceA` with `SERVICE_AUTO_START`, `StartServiceA`. Uninstall stops the service, waits up to 10 s for `SERVICE_STOPPED`, then `DeleteService`. Both handled idempotently (already installed / already running / not installed).

### 13.3 Automatic Mode Detection

| Platform | Terminal | Service |
|----------|----------|---------|
| Linux | Same code path (signal handlers work either way) | systemd drives `run()` via `sd_notify` |
| Windows | SCM dispatch fails → foreground fallback | SCM drives `ServiceMain` |

Neither platform requires `--foreground`. The flag is accepted and ignored for symmetry.

---

## 14. Configuration System

### 14.1 Config Path Resolution (Bug Fix #13)

`Application::resolve_config_path()` uses a three-step fallback:

1. If `--config <path>` was given, use it
2. If `./config/agent.default.json` exists, use it (dev workflow)
3. Use `paths::default_config_path()` (the platform default)

This fixes the "Config File Not Found When Running as Service" limitation: systemd starts the agent with CWD=/, so step 2 fails and step 3 produces `/etc/axiomdlp/agent.json`.

### 14.2 Current Schema

```json
{
  "logging": {
    "dir":   "logs",
    "file":  "agent.log",
    "level": "info"
  },
  "modules": {
    "filesystem": {
      "enabled": true,
      "watch_paths": ["/home", "/tmp", "/var/tmp", ...],
      "exclude_paths": ["*/node_modules/*", "*/.git/*", ...],
      "debounce_ms": 300,
      "max_file_size_mb": 100
    }
  }
}
```

### 14.3 Field Reference

| Field | Type | Default | Notes |
|-------|------|---------|-------|
| `logging.dir` | string | `"logs"` | Created if missing |
| `logging.file` | string | `"agent.log"` | Relative to `dir` |
| `logging.level` | string | `"info"` | One of: trace, debug, info, warn, error, critical, off |
| `modules.filesystem.enabled` | bool | `false` | Master switch for the module |
| `modules.filesystem.watch_paths` | string[] | `[]` | Recursive directories |
| `modules.filesystem.exclude_paths` | string[] | `[]` | Glob or directory prefix |
| `modules.filesystem.debounce_ms` | int ≥ 0 | `300` | Trailing-edge window |
| `modules.filesystem.max_file_size_mb` | int ≥ 0 | `100` | Reserved for Scanner |

### 14.4 Fail-Safe Behavior

| Scenario | Behavior |
|----------|----------|
| File missing | Warning + built-in defaults |
| Invalid JSON | Warning + built-in defaults |
| Root not an object | Warning + built-in defaults |
| Wrong type for a field | Warning + default for that field only |
| Invalid log level | Warning + `"info"` |
| Negative integer | Warning + clamped default |
| Unknown key | Ignored silently |

**The agent always starts.** Config problems produce warnings, not failures.

### 14.5 JSON Schema

`config/agent.schema.json` describes the full schema for editors (VS Code IntelliSense). It matches the shipped defaults exactly after the bug fix pass.

---

## 15. Logging Subsystem

### 15.1 Setup

`Logger::init(dir, file, level)`:

1. Creates the log directory if missing
2. Initializes spdlog's async thread pool: 8192 slots, 1 worker
3. Creates a colored console sink (`stdout_color_sink_mt`)
4. Creates a rotating file sink: **5 MB per file, 3 rotated files** (≈20 MB cap)
5. Combines into an `async_logger` with `overrun_oldest` policy
6. Sets the pattern: `[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [t:%t] %v`
7. Sets level from config string
8. `flush_on(warn)` — warnings and above flush immediately
9. Registers as the global default logger

### 15.2 Non-Blocking Guarantee

`overrun_oldest` means: if the queue is full, drop the oldest message rather than block the caller. This means a `SPDLOG_INFO` from a hot path (like a filesystem callback) never stalls the producer.

### 15.3 Shutdown

`Logger::shutdown()` calls `spdlog::shutdown()`, which drains the async queue and joins the worker. Called from `Application::~Application()`, so log messages are never lost on a clean exit.

### 15.4 Log Levels in Use

| Level | Used For |
|-------|----------|
| `TRACE` | Not currently used |
| `DEBUG` | Every event the bus carries (EventLoggerModule) |
| `INFO` | Lifecycle: startup, module register/init/start/stop/shutdown, watcher start/stop |
| `WARN` | Config problems, non-fatal errors, partial failures |
| `ERROR` | Module init failures, watcher start failure |
| `CRITICAL` | Not currently used |

Default config level is `info`, so `DEBUG` messages are hidden in production.

---

## 16. Bug Fixes Applied

This section lists every bug found in the code review and how it was fixed. If you're returning to this project later, these are the bugs you no longer need to worry about.

### Fix #1 — Log Feedback Loop

**Bug:** `EventLoggerModule` writes to `logs/agent.log`. The filesystem module watches the parent directory. Every log line becomes a new `file_write` event → new log line → infinite loop at ~3 Hz.

**Fix:** `FilesystemModule::build_excludes()` always appends the absolute path of the configured log directory, with `/*`, to the exclusion list.

**Verification:** `agent.log` no longer appears as an event path in the runtime log.

### Fix #2 — `exclude_paths`, `debounce_ms`, `max_file_size_mb` Silently Ignored

**Bug:** Config files declared these fields, but `FilesystemModuleConfig` only had `enabled` and `watch_paths`. The config loader never read the other keys.

**Fix:** Added all three to `FilesystemModuleConfig`. Added a `read_int` helper to `config.cpp`. Clamping for negatives. `debounce_ms` is now wired into the module; `max_file_size_mb` is stored for the Scanner milestone.

### Fix #3 — Debounce Lost `modified` When Followed by `deleted`

**Bug:** Debounce overwrote `event = new_event` unconditionally. A create → modify → delete within the window emitted only `deleted`.

**Fix:** Priority table (`modified` 4 > `created` 3 > `moved` 2 > `deleted` 1). Overwrite only when `new_priority >= existing_priority`. Deadline always extends.

### Fix #4 — Event Storm from `/`, `/etc`, `/usr/local`

**Bug:** Default Linux config watched system directories, generating thousands of events per hour of no DLP value.

**Fix:** Narrowed default `watch_paths` to `/home`, `/tmp`, `/var/tmp`, `/media`, `/mnt`, `/opt`, `/srv`, `/data`, `/workspace`. Added `*/logs/*` to default excludes.

### Fix #6 — EventLogger at INFO Amplified the Loop

**Bug:** Every event logged at `INFO`.

**Fix:** Downgraded to `SPDLOG_DEBUG`. Simplified format (dropped `id=` which was only useful in deep debugging).

### Fix #7 — `FilesystemModule::health()` Null Dereference

**Bug:** `health()` unconditionally dereferenced `ctx_`. If `initialize()` failed or was never called, `ctx_` is null. `ModuleManager::health()` iterates all modules, so this was reachable.

**Fix:** Guard `ctx_ == nullptr` → return `HealthStatus::unknown("not initialized")`. Also added the `HealthStatus::unknown()` factory, which was missing.

### Fix #9 — `Application::init()` Misleading Return

**Bug:** The function warned on module init failure but always returned `true`. This made the return value meaningless.

**Fix:** Kept fail-safe behavior but changed the log level to `ERROR` and made the comment explicit that returning `true` is intentional.

### Fix #10 — `--install` / `--uninstall` Ran Full Init

**Bug:** `execute()` always called `init()` before checking `opts_.install`. This created log directories, registered modules, and initialized them for a one-shot command.

**Fix:** Moved install/uninstall handling before `init()`. Uses a minimal `install.log` for those commands. No module tree, no event bus.

### Fix #13 — `paths::default_config_path()` Not Used

**Bug:** `parse_args` hardcoded the relative dev path. When running as a service (CWD=/) the config was never found, silently using built-in defaults.

**Fix:** `resolve_config_path()` with a three-step chain: explicit `--config` → CWD-relative dev path → platform default.

### Fix #16 — `install_service` Relied on `$PATH` for `systemctl`

**Bug:** `execvp("systemctl", ...)` searches `$PATH`, which is unreliable in minimal containers and hardened systemd environments.

**Fix:** `find_systemctl()` probes `/usr/bin/systemctl`, `/bin/systemctl`, `/usr/local/bin/systemctl` before falling back to `$PATH`.

### Fix #17 — `is_safe_service_name` Allowed `..`

**Bug:** The name validator allowed `.`, so `".."` passed. This is a path-traversal vector, even if not currently user-controlled.

**Fix:** Explicitly reject `..`, leading `.`, and trailing `.`.

### Fix (Bonus) — Directory Events in the Pending Map

Not a numbered bug, but worth documenting: efsw fires `modified` on directories when their entry list changes. These are pure noise. `on_file_event` now returns early when `fe.is_directory == true`.

---

## 17. Runtime Evidence — What the Logs Prove

The `logs/agent.log` from 2026-09-24 demonstrates:

### 17.1 Clean Startup and Shutdown

```
AxiomDLP Agent v0.1.0
Config loaded from: config/agent.default.json
ModuleManager: registering module 'example' v1.0.0
ModuleManager: registering module 'filesystem' v1.0.0
ModuleManager: registering module 'event_logger' v1.0.0
ModuleManager: initializing 3 module(s)
...
ModuleManager: starting 3 module(s)
...
sd_notify READY sent
...
axiomdlp-agent received stop request
ModuleManager: stopping 3 module(s)
ModuleManager: stopping 'event_logger'
ModuleManager: stopping 'filesystem'
ModuleManager: stopping 'example'
ModuleManager: shutting down 3 module(s)
...
Service returned rc=0
Application shutting down
```

Every step in the documented lifecycle appears in the log. Stop order is verified as reverse of start order.

### 17.2 Partial Watcher Failure Handled Gracefully

```
EfswFileWatcher: cannot watch '/' (permission denied, missing, or unsupported); skipping
EfswFileWatcher: cannot watch '/data' (permission denied, missing, or unsupported); skipping
EfswFileWatcher: cannot watch '/workspace' (permission denied, missing, or unsupported); skipping
EfswFileWatcher: watching '/root' (id=1)
EfswFileWatcher: watching '/home' (id=162)
...
EfswFileWatcher: started, watching 11 path(s) (3 skipped)
filesystem module: started (debounce 300 ms)
```

A failing path does not fail the watcher. 11 of 14 succeeded and the module started normally.

### 17.3 Debounce Works

Compare 12:32:31 (before debounce) with 12:45:44 (after):

**Before (12:32):**
```
[event] type=file_write id=... path=/tmp/dlp-test//file1.txt
[event] type=file_write id=... path=/tmp/dlp-test//file1.txt
[event] type=file_write id=... path=/tmp/dlp-test//file1.txt
```

3 kernel events → 3 bus events.

**After (12:45):**
```
[event] type=file_write id=1790235944091596688-1 path=/tmp/dlp-test/file1.txt
```

3 writes → 1 bus event. Also note the double-slash bug was fixed between these runs.

### 17.4 Path Normalization Fixed

Early logs: `/tmp/dlp-test//file1.txt` (double slash from efsw's `dir` + `filename` concatenation).
Later logs: `/tmp/dlp-test/file1.txt` (single slash from the trailing-slash check in the callback).

### 17.5 Real File Activity Captured

```
[event] type=file_write path=/home/kali/.config/google-chrome/Profile 9/Session Storage/000138.log
[event] type=file_write path=/home/kali/.config/Code/logs/20260924T083251/window1/exthost/vscode.git/Git.log
[event] type=file_delete path=/tmp/.com.google.Chrome.cY8Gfc
```

Chrome, VS Code, and temp files are being detected, debounced, and published. The pipeline is live.

---

## 18. Known Limitations

### 18.1 No Process or User Attribution

`Event::user_id`, `process_name`, `pid` are never populated. `FileEvent` doesn't carry them, and efsw doesn't provide them. This is a fundamental limitation of the current watcher backend.

**Impact:** For DLP, attribution is essential. Without it, the policy engine cannot distinguish "Alice copied her own file" from "Bob copied Alice's file."

**Planned fix:** Kernel enforcement milestone will need `fanotify` (Linux) or a minifilter (Windows). These provide process context per file operation. Alternatively, a `/proc/<pid>/fd` scan on Linux can be used to correlate inode → process.

### 18.2 Windows Config Path Under Service

When installed as a Windows Service, the process runs as `LOCAL SYSTEM` with `%ProgramData%` resolving correctly. But the config path resolution chain still prefers `./config/agent.default.json` if the CWD happens to contain it. This is unlikely but possible if the service is started manually from an unusual directory.

**Planned fix:** For service mode, prefer `paths::default_config_path()` unconditionally. Currently, the dev-relative path is checked first, which is correct for development but slightly surprising for a service.

### 18.3 No Windows Ctrl+C Handler

Pressing Ctrl+C in Windows foreground mode doesn't stop the agent cleanly. The SCM dispatch fails (as designed), the foreground fallback runs, but there is no `SetConsoleCtrlHandler` installed. The user must close the terminal or use Task Manager.

**Planned fix:** Add `SetConsoleCtrlHandler` in `service_win.cpp`'s `run_foreground()`, setting `g_stop` on `CTRL_C_EVENT` and `CTRL_CLOSE_EVENT`.

### 18.4 `debounce_ms` Lower Bound Not Enforced

Any non-negative value is accepted. `0` means "no debounce," which restores the pre-fix behavior of one event per kernel event. The clamp only enforces `>= 0`. A minimum of 50 ms would be safer.

### 18.5 Module Shutdown Clears the Registry

`ModuleManager::shutdown_all()` calls `modules_.clear()`. After this, `count()` returns 0 and no modules can be restarted. This is correct for process-exit shutdown, but it blocks any future "reload modules at runtime" feature.

### 18.6 No Module Init Failure Diagnostic in `Application`

`Application::init()` reports the total count of failed modules but not which ones. The `ModuleManager` logs each failure individually, so the information is in the log, but the summary is not surfaced in the return path.

### 18.7 Config Schema Not Validated at Runtime

The `agent.schema.json` file exists but is not consulted by `config.cpp`. Typos in field names silently fall back to defaults. Consider adding a schema validation step using `nlohmann::json_schema` or a hand-rolled check for unknown keys within known sections.

### 18.8 Windows Path Matching Case Sensitivity

`glob_match` is case-sensitive. Windows filesystems are case-insensitive. A pattern `C:\Windows\*` will not match a path spelled `c:\windows\...`. This may bite on Windows in practice.

**Planned fix:** Normalize both pattern and path to lowercase inside `path_matches_pattern` under `#ifdef _WIN32`.

### 18.9 Event Bus Is Synchronous

`publish` calls every matching handler on the calling thread. A slow subscriber delays the publisher. Today the only publisher is a single watcher thread, so the cost is bounded. As the Scanner, PolicyEngine, and Storage subscribe, this may become a bottleneck.

**Planned fix:** Async publish with a worker queue. Contained inside `EventBus::publish`.

---

## 19. What Does Not Exist Yet

| Feature | Milestone | Notes |
|---------|-----------|-------|
| Local regex scanner | 8 | Credit cards, SSNs, emails, API keys |
| Remote PII scanner | 8 | Calls AxiomDLP Core's `/pii/detect` |
| Unified scan result type | 8 | `ScanResult`, `ScanEntity` |
| Policy engine | 9 | Evaluates events against policies |
| Local policy cache | 9 | Initially hardcoded, later SQLite |
| Decision type | 9 | Block / alert / log / allow |
| SQLite storage | 10 | Events, policies, state, audit log |
| Migration system | 10 | Schema versioning |
| User-mode enforcement | 11 | Block clipboard or file operations |
| IPC server | 12 | Length-prefixed JSON over named pipe / Unix socket |
| Tray helper | 12 | Separate process, runs in user session |
| HTTP client | 13 | libcurl |
| WebSocket client | 13 | Boost.Beast |
| Enrollment | 13 | Agent registers with core |
| Heartbeat | 13 | Every 30–60 seconds |
| Event upload | 13 | Batched, offline-queued |
| Policy sync | 13 | Fetch policies from core |
| Reconnect with backoff | 13 | Exponential with jitter |
| Kernel enforcement (Windows) | 14 | Minifilter driver |
| Kernel enforcement (Linux) | 14 | eBPF LSM or fanotify |

---

## 20. Commands Reference

### 20.1 Build

| Task | Command |
|------|---------|
| Build all four targets | `./scripts/build-all.sh` |
| Build one target | `cmake --build --preset linux-x86_64-release` |
| Configure one target | `cmake --preset linux-x86_64-release` |
| Clean build | `rm -rf build && ./scripts/build-all.sh` |
| Clean dependency cache | `rm -rf .deps` (forces re-download) |
| Regenerate clangd DB | `./scripts/merge-compile-commands.sh` |

### 20.2 Run

| Task | Command |
|------|---------|
| Foreground (dev) | `./build/linux-x86_64-release/axiomdlp-agent --foreground` |
| With custom config | `./build/linux-x86_64-release/axiomdlp-agent --foreground --config /path/to/config.json` |
| Install as service | `sudo ./build/linux-x86_64-release/axiomdlp-agent --install` |
| Uninstall service | `sudo ./build/linux-x86_64-release/axiomdlp-agent --uninstall` |

### 20.3 Observe

| Task | Command |
|------|---------|
| Follow log file | `tail -f logs/agent.log` |
| Follow journald logs | `sudo journalctl -u axiomdlp-agent -f` |
| Check service status | `sudo systemctl status axiomdlp-agent` |
| Check Windows service | `sc query AxiomDLPAgent` |

### 20.4 Command-Line Flags

| Flag | Effect |
|------|--------|
| `--foreground` | Accepted; on Windows forces foreground fallback |
| `--install` | Install as a service, then exit |
| `--uninstall` | Remove the service, then exit |
| `--config <path>` | Use a specific config file |

**Not yet implemented:** `--version`, `--log-level`, `--no-modules`, `--module <name>`.

---

## 21. Verification Checklist

To confirm the current state end-to-end, run through this list on a fresh build.

### 21.1 Build Verification

- [ ] `./scripts/build-all.sh` completes without errors
- [ ] `build/linux-x86_64-release/axiomdlp-agent` exists and is executable
- [ ] `build/linux-i686-release/axiomdlp-agent` exists and is 32-bit (`file` shows ELF 32-bit)
- [ ] `build/windows-x86_64-release/axiomdlp-agent.exe` exists and is a PE32+ executable
- [ ] `build/windows-i686-release/axiomdlp-agent.exe` exists and is a PE32 executable
- [ ] `file` on the Windows binaries shows no dynamic DLL dependencies other than system ones

### 21.2 Runtime Verification (Linux Foreground)

- [ ] `./axiomdlp-agent --foreground` starts and logs "Config loaded from:"
- [ ] All three modules register, initialize, and start
- [ ] `sd_notify READY sent` appears (if built with libsystemd)
- [ ] Ctrl+C stops the process cleanly, with stop order reversed from start order

### 21.3 Filesystem Event Verification

- [ ] Create a file in a watched directory → one `file_write` event
- [ ] Modify the file → one `file_write` event (debounced)
- [ ] Delete the file → one `file_delete` event
- [ ] Create, modify, delete within 300 ms → one `file_write` event (priority preserved)
- [ ] Create a file in an excluded path → no event
- [ ] `agent.log` itself never appears as an event path

### 21.4 Config Verification

- [ ] Missing config file → warning + defaults, agent still starts
- [ ] Malformed JSON → warning + defaults, agent still starts
- [ ] Wrong type for `debounce_ms` → warning, uses default
- [ ] Negative `debounce_ms` → warning, uses 300
- [ ] Invalid log level → warning, uses `info`

### 21.5 Service Verification (Linux)

- [ ] `sudo ./axiomdlp-agent --install` writes the unit file
- [ ] `systemctl status axiomdlp-agent` shows active (running)
- [ ] `journalctl -u axiomdlp-agent -f` shows logs
- [ ] `sudo ./axiomdlp-agent --uninstall` stops, disables, and removes cleanly
- [ ] No orphan process after uninstall

### 21.6 Service Verification (Windows)

- [ ] `axiomdlp-agent.exe --install` (as Administrator) creates the service
- [ ] `sc query AxiomDLPAgent` shows `RUNNING`
- [ ] `axiomdlp-agent.exe --uninstall` removes it cleanly
- [ ] Running without `--install` from a terminal falls back to foreground

---

## 22. Next Milestones

In order, with a one-sentence description of each.

### Milestone 8 — Scanner

Subscribe to file events, read the file content (bounded by `max_file_size_mb`), run regex patterns for credit cards, SSNs, emails, phone numbers, and API keys, and publish `scan_result` events. This is where the DLP pipeline starts producing real output.

### Milestone 9 — Policy Engine

Subscribe to `scan_result` events, evaluate them against local policies, and publish `decision` events. Introduces the `Policy`, `Decision`, and `PolicyEngine` types.

### Milestone 10 — Storage

SQLite persistence for events, decisions, policies, and state. Runs migrations on startup. Introduces the event queue that survives restarts.

### Milestone 11 — User-Mode Enforcement

Subscribe to `decision` events and act on them: block file writes, block clipboard copies. Introduces `IEnforcer` and the enforcement registry.

### Milestone 12 — IPC + User Helper

Named pipe / Unix socket server in the service, tray UI client in the user session. Introduces the `IIpc` interface and message schema.

### Milestone 13 — Transport

HTTP + WebSocket client for enrollment, heartbeat, event upload, and policy sync. Introduces `ITransport` and the protocol layer.

### Milestone 14 — Kernel Enforcement

Windows Minifilter driver and Linux eBPF LSM or fanotify with permission events. User-mode policy evaluation remains; kernel-mode enforcement acts on decisions.

---

## Summary

**The foundation is complete and production-quality.** Every subsystem built so far follows the same discipline:

- Interface in `core/`, implementation in `platform/<os>/` or `modules/<name>/`
- Fail-safe by default: bad config, missing paths, failed modules never crash the agent
- RAII for all resources
- Thread-safe pub/sub with per-handler exception isolation
- Debounced, filtered, prioritized filesystem events
- Zero OS headers in `core/` or `modules/`
- Four cleanly cross-compiled targets from one Linux host
- Service install, run, and uninstall verified on Linux; install and run logic in place on Windows

**What runs today:** a monitoring agent that watches the filesystem, debounces the noise, filters the irrelevant, and publishes clean events to an in-process bus, where an observability module logs them.

**What comes next:** the Scanner, which turns those events into DLP signal.

The pipeline shape — `Capture → Scan → Decide → Enforce → Report` — is fully specified in `docs/architecture.md`. Only the first stage is implemented. The other four have clean insertion points waiting for them.

**The agent is no longer a vessel with no cargo.** It has cargo. It just doesn't yet know what to do with it.