# AxiomDLP Endpoint Agent — DONE.md

**Status Document — Everything Implemented, Verified, and Fixed on the Agent Side**

**Last updated:** After atomic ShutdownController, bidirectional WebSocket, server-pushed revocation, TLS enforcement, config consolidation, and industry-standard timing.

**Supersedes:** all previous DONE.md revisions.

---

## 1. Executive Summary

The AxiomDLP Endpoint Agent is a **cross-platform, TLS-capable, network-connected service foundation** with four verified build targets, six working modules, a persistent local SQLite database, SHA-256 file fingerprinting, HTTP + WebSocket connectivity to the AxiomDLP Core server, a content scanner for text-based PII detection, and an **atomic ShutdownController** that lets any component stop the whole agent cleanly with a typed reason.

### What Works Today

- Four build targets from one Linux host: Linux x86_64, Linux i686, Windows x86_64, Windows i686.
- Cross-platform service lifecycle (systemd on Linux, SCM on Windows).
- Async structured logging with rotation and non-blocking delivery.
- JSON configuration with fail-safe defaults, environment variable expansion, and a single top-level `server` section for all server-facing values.
- Real-time filesystem watching via inotify (Linux) and ReadDirectoryChangesW (Windows) using efsw.
- Periodic full-tree inventory scanning with interruptible cancellation.
- SHA-256 content hashing of eligible files, with metadata stored in SQLite.
- **Bidirectional WebSocket** to the Core server with TLS, jittered reconnect backoff, close-code interpretation, and graceful 1000 Normal close frames.
- HTTP client supporting `http://` and `https://` with unconditional certificate verification.
- Agent enrollment with the Core server via `POST /api/v1/agent/register`.
- Periodic heartbeat via `POST /api/v1/agent/heartbeat`, plus an immediate validation heartbeat at startup.
- **Atomic shutdown**: any module can request a shutdown with a typed reason (`UserRequested`, `AuthRevoked`, `AgentRevoked`, `FatalError`, `ConfigError`, `ServerShutdown`), and the whole agent tears down in reverse order, logging the reason end to end.
- **Server-pushed revocation handling**: the agent reacts to `token_revoked` and `agent_revoked` messages by requesting a full shutdown.
- **Goodbye message** on graceful exits: `{"type":"agent_shutdown","payload":{"reason":...}}` is sent to the server before the WebSocket is closed.
- **Content scanner**: reads text files, splits them into overlapping chunks, sends batches to `POST /api/v1/agent/scan/batch`, and writes the merged verdict back to the local `files` table.
- Statically linked OpenSSL 3.3.2 for all four targets.
- Local visualization dashboard (Python server + HTML page) for development.

### What Does Not Exist Yet

- Policy engine (decisions from scan results).
- Enforcement (blocking file operations in user mode).
- Kernel-mode enforcement (Windows Minifilter / Linux eBPF / fanotify).
- IPC + user helper (tray icon, notifications).
- Auto-update mechanism.
- Code signing and packaging (`.deb`, `.rpm`, MSI).
- Rich-document extraction inside the content scanner (PDF, DOCX, XLSX, images).

The agent is a **complete capture, inventory, fingerprinting, content-scanning, connectivity, and shutdown-management platform** with no decision-making or enforcement layer yet.

---

## 2. The Four Verified Platforms

All four targets build, link, and run. All support TLS and the full bidirectional WebSocket pipeline.

| Platform | Build | Run | Watch | Hash | HTTP | WSS | Content Scanner | Shutdown |
|----------|-------|-----|-------|------|------|-----|-----------------|----------|
| **Linux x86_64** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Linux i686** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Windows x86_64** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Windows i686** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## 3. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        APPLICATION                                   │
│  Owns: Config, Logger, ShutdownController, Database, FileStore,      │
│        StateStore, EventBus, ModuleContext, ModuleManager, Service   │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          SERVICE                                     │
│  Linux: systemd + sd_notify + signals                                │
│  Windows: SCM + ServiceMain + control handler                        │
│  Blocks on ShutdownController::wait(); tears down on request()       │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       MODULE MANAGER                                 │
│  Lifecycle: initialize → start → stop → shutdown (reverse order)     │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
    ┌──────────────┬───────────┼───────────┬──────────────┬──────────┐
    ▼              ▼           ▼           ▼              ▼          ▼
┌─────────┐  ┌──────────┐  ┌────────┐  ┌────────────┐  ┌─────────┐  ┌────────┐
│Filesystem│ │Inventory │ │Event   │  │WebSocket   │  │AgentSync│  │Content │
│Watcher  │  │Module    │ │Logger  │  │Transport   │  │Module   │  │Scanner │
└────┬────┘  └─────┬────┘ └───┬────┘  └─────┬──────┘  └────┬────┘  └────────┘
     │             │          │             │              │
     │ publish     │ hash     │ subscribe   │ WS + TLS     │ HTTP + TLS
     ▼             ▼          ▼             ▼              ▼
┌─────────┐  ┌──────────┐  ┌────────┐  ┌──────────┐  ┌──────────────┐
│EventBus │  │FileStore │  │EventBus│  │Core WS  │  │Core Server    │
│(pub/sub)│  │(SQLite)  │  │        │  │Endpoint │  │(register +    │
│         │  │          │  │        │  │          │  │ heartbeat +   │
│         │  │          │  │        │  │          │  │ scan/batch)   │
└─────────┘  └──────────┘  └────────┘  └──────────┘  └──────────────┘

          ShutdownController (shared by every module and by IService)
          ─────────────────────────────────────────────────────────
          request(reason)  ←  any module, any time, first wins
          wait()           ←  service main loop blocks here
          reason()         ←  read by transport to decide goodbye
```

**Design principles enforced:**

1. **Layered source tree** — `foundation/` ← `events/` ← `modules/` ← `features/`. Higher layers depend on lower; lower never on higher.
2. **Interface in `platform/`, implementation in `platform/<os>/`** — business logic never includes OS headers.
3. **Every capability is a module** — new DLP features are added by creating `features/<name>/` and registering in `application.cpp`.
4. **Modules communicate through the event bus** — never directly.
5. **One shutdown authority** — the `ShutdownController` is the only place shutdown is requested, and every module reads the same reason.
6. **Fail-safe** — bad config, missing paths, failed modules never crash the agent.
7. **RAII everywhere** — destructors clean up, no manual resource management.

---

## 4. Source Tree Layout

```
axiomdlp-agent/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── DONE.md                            (this file)
├── LICENSE
├── .clangd / .clang-tidy / .clang-format
├── .gitignore
│
├── .deps/                             Cached third-party (gitignored)
│   ├── spdlog-src/
│   ├── nlohmann_json-src/
│   ├── efsw-src/
│   ├── sqlite3-src/
│   ├── openssl-src/
│   ├── openssl-linux-x86_64/
│   ├── openssl-linux-i686/
│   ├── openssl-mingw64/
│   ├── openssl-mingw32/
│   └── boost-include/
│
├── cmake/toolchains/
│   ├── linux-x86_64.cmake
│   ├── linux-i686.cmake
│   ├── windows-x86_64.cmake
│   └── windows-i686.cmake
│
├── config/
│   ├── agent.default.json
│   ├── agent.default_win.json
│   └── agent.schema.json
│
├── data/                              (gitignored)
├── logs/                              (gitignored)
│
├── scripts/
│   ├── build-all.sh
│   ├── build-openssl.sh
│   ├── merge-compile-commands.sh
│   ├── mock_server.py
│   ├── viz_server.py
│   └── viz.html
│
└── src/
    ├── main.cpp

    ├── foundation/
    │   ├── application.{h,cpp}        Orchestration, arg parsing
    │   ├── config.{h,cpp}             JSON load + validation + env expansion
    │   ├── logger.{h,cpp}             Async spdlog setup
    │   └── shutdown.h                 ShutdownController + ShutdownReason

    ├── events/
    │   ├── event.{h,cpp}
    │   └── event_bus.{h,cpp}

    ├── modules/
    │   ├── i_module.h
    │   ├── module_base.h
    │   ├── module_context.h           Now holds ShutdownController&
    │   ├── module_manager.{h,cpp}
    │   └── health.h

    ├── platform/
    │   ├── paths.h
    │   ├── service.h                  Now takes ShutdownController
    │   ├── file_watcher.h
    │   ├── common/
    │   │   └── efsw_file_watcher.cpp
    │   ├── linux/
    │   │   ├── paths.cpp
    │   │   └── service.cpp            Waits on ShutdownController
    │   └── windows/
    │       ├── paths.cpp
    │       └── service.cpp            Waits on ShutdownController

    ├── net/
    │   ├── http_client.{h,cpp}        Always verifies TLS
    │   ├── websocket_client.{h,cpp}   No per-read deadline, jitter, graceful close
    │   ├── outbound_queue.{h,cpp}
    │   └── message_router.{h,cpp}

    ├── util/
    │   ├── glob_match.{h,cpp}
    │   ├── file_hash.{h,cpp}
    │   └── base64.{h,cpp}

    ├── storage/
    │   ├── sqlite_database.{h,cpp}
    │   ├── migrations.{h,cpp}
    │   ├── file_repository.{h,cpp}
    │   └── state_repository.{h,cpp}

    └── features/
        ├── filesystem_watcher/
        │   └── filesystem_watcher_module.{h,cpp}
        ├── inventory/
        │   ├── inventory_module.{h,cpp}
        │   └── inventory_scanner.{h,cpp}
        ├── event_logger/
        │   └── event_logger_module.{h,cpp}
        ├── websocket_transport/
        │   └── transport_module.{h,cpp}
        ├── agent_sync/
        │   └── agent_sync_module.{h,cpp}
        └── content_scanner/
            └── content_scanner_module.{h,cpp}
```

---

## 5. Build System

### Prerequisites (Kali/Debian/Ubuntu)

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y \
    build-essential gcc-multilib g++-multilib \
    mingw-w64 ninja-build git file clangd \
    libboost-dev libsystemd-dev libsystemd-dev:i386
```

### Build All Four Targets

```bash
cd ~/axiomdlp-agent
./scripts/build-all.sh
```

**First run:** downloads and compiles OpenSSL 3.3.2 for all four targets (~15–25 min). Subsequent runs: incremental (~30 seconds).

### Presets

| Preset | Output | Compiler |
|--------|--------|----------|
| `linux-x86_64-{debug,release}` | `build/linux-x86_64-*/axiomdlp-agent` | `/usr/bin/g++` |
| `linux-i686-{debug,release}` | `build/linux-i686-*/axiomdlp-agent` | `/usr/bin/g++ -m32` |
| `windows-x86_64-{debug,release}` | `build/windows-x86_64-*/axiomdlp-agent.exe` | `x86_64-w64-mingw32-g++` |
| `windows-i686-{debug,release}` | `build/windows-i686-*/axiomdlp-agent.exe` | `i686-w64-mingw32-g++` |

### Dependencies

| Library | Version | Purpose | Source |
|---------|---------|---------|--------|
| spdlog | 1.14.1 | Async logging | FetchContent + `.deps/` |
| nlohmann/json | 3.11.3 | JSON parsing | System on Linux; FetchContent on Windows |
| efsw | 1.7.2 | File watching | FetchContent + `.deps/` |
| SQLite | 3.46.1 | Metadata storage | Amalgamation + `.deps/` |
| Boost | 1.90 | HTTP + WebSocket | System headers, isolated include |
| OpenSSL | 3.3.2 | TLS + hashing + base64 | Built from source for all 4 targets |
| libsystemd | (system) | `sd_notify` | pkg-config (Linux only, optional) |

---

## 6. Foundation Layer

### `ShutdownController` — `src/foundation/shutdown.h`

The single, thread-safe authority on whether the agent is shutting down and why.

```cpp
enum class ShutdownReason {
    None,
    UserRequested,      // Ctrl+C, SIGTERM, service stop
    AuthRevoked,        // 401 from server: token invalid or revoked
    AgentRevoked,       // 403 from server: this specific agent revoked
    ConfigError,
    FatalError,
    ServerShutdown      // server sent 1001 Going Away
};

class ShutdownController {
public:
    bool request(ShutdownReason reason);   // first call wins
    void wait();                            // blocks until requested
    bool wait_for(duration);                // bounded wait
    bool is_requested() const;
    ShutdownReason reason() const;
};
```

- First caller wins; later calls are no-ops.
- Any module can request at any time.
- Service main loop blocks on `wait()` and tears down when a request arrives.
- The reason is preserved end to end (final log line, goodbye message, server log).

### `Application` — `src/foundation/application.{h,cpp}`

Owns the entire lifecycle:

1. Create the `ShutdownController` first.
2. Parse arguments (`--install`, `--uninstall`, `--foreground`, `--config`).
3. Resolve config path with fallback chain.
4. Load config (fail-safe; warnings collected).
5. Initialize logger.
6. Open SQLite database.
7. Apply migrations.
8. Create `FileStore`, `StateStore`.
9. Create `EventBus`.
10. Create `ModuleContext` (now includes the shutdown controller).
11. Create `ModuleManager`, register 6 modules.
12. Initialize all modules.
13. For install/uninstall: short-circuit.
14. For normal run: create platform `IService`, wire manager and controller, run.

**Shutdown sequence:** destroys members in reverse dependency order **before** `Logger::shutdown()` to avoid the segfault that happened when `Database::~Database()` logged through a dead spdlog singleton.

### `Config` — `src/foundation/config.{h,cpp}`

Fail-safe JSON loading with a **single top-level `server` section**:

```json
"server": {
  "base_url": "http://127.0.0.1:3000",
  "api_token": "axdlp_user_...",
  "heartbeat_interval_seconds": 60,
  "reconnect_backoff_seconds": 10,
  "reconnect_backoff_max_seconds": 300
}
```

- All server-facing values live here. `agent_sync`, `transport`, and `content_scanner` read from the same place.
- `verify_tls` is no longer parsed anywhere — TLS is always verified on `https://` and `wss://`.
- Legacy fields under `modules.agent_sync.*` and `modules.transport.*` are still read (with a deprecation warning) so old configs keep working.
- Environment variable expansion: `%VAR%` (Windows) and `${VAR}` (POSIX).
- Never throws; every field has a safe default.

### `Logger` — `src/foundation/logger.{h,cpp}`

- spdlog async logger.
- 8192-slot queue, 1 background worker.
- Console sink (colored) + rotating file sink (5 MB × 3).
- `overrun_oldest` policy — never blocks the caller.
- Format: `[timestamp] [level] [thread-id] message`.
- Flush on `warn` and above.

---

## 7. Events Layer

### `Event` — `src/events/event.{h,cpp}`

```cpp
struct Event {
    std::string id;
    std::string type;
    std::chrono::system_clock::time_point timestamp;
    std::string user_id;
    std::string process_name;
    int         pid = 0;
    std::map<std::string, std::string> metadata;
    std::optional<std::string> content;
};
```

**Canonical event types:** `file_access`, `file_write`, `file_delete`, `usb_insert`, `usb_remove`, `clipboard_copy`, `print_job`, `process_spawn`, `scan_result`, `decision`.

### `EventBus` — `src/events/event_bus.{h,cpp}`

In-process synchronous publish/subscribe. `shared_ptr<Event>` is shared with all subscribers. Handlers are invoked outside the lock, so they can safely subscribe or unsubscribe during a callback.

---

## 8. Module Framework

### `IModule` — `src/modules/i_module.h`

```cpp
class IModule {
public:
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual bool initialize(ModuleContext&) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void shutdown() = 0;
    virtual HealthStatus health() const = 0;
};
```

### `ModuleContext` — `src/modules/module_context.h`

```cpp
struct ModuleContext {
    const Config&       config;
    EventBus&           event_bus;
    FileStore&          file_store;
    StateStore&         state_store;
    Database&           database;
    ShutdownController& shutdown;     // NEW
};
```

### `ModuleManager` — `src/modules/module_manager.{h,cpp}`

Owns `std::vector<std::unique_ptr<IModule>>`. Start in forward order; stop and shutdown in reverse. Aggregates health.

### `HealthStatus` — `src/modules/health.h`

Four levels: `Unknown`, `OK`, `Degraded`, `Failed`.

---

## 9. Platform Layer

### Interfaces

| File | Purpose |
|------|---------|
| `platform/paths.h` | Executable path, config path, log dir, data dir |
| `platform/service.h` | `IService` with `set_shutdown_controller`, `request_stop(reason)` |
| `platform/file_watcher.h` | `IFileWatcher` + `FileEvent` |

### Linux — `platform/linux/`

- `paths.cpp` — `readlink("/proc/self/exe")`, `/etc/axiomdlp/`, `/var/log/axiomdlp/`, `/var/lib/axiomdlp/`.
- `service.cpp` — systemd, `sd_notify`, signals. Signal handler sets an atomic flag; main loop translates it into a `ShutdownController` request.

### Windows — `platform/windows/`

- `paths.cpp` — `GetModuleFileNameA`, `SHGetKnownFolderPath`.
- `service.cpp` — SCM, `ServiceMain`, control handler. `SERVICE_CONTROL_STOP` → `ShutdownController::request(UserRequested)`.

---

## 10. Net Layer

### `HttpClient` — `src/net/http_client.{h,cpp}`

Synchronous HTTP/HTTPS client built on Boost.Beast.

- POST with JSON body and optional Bearer token.
- Supports `http://` and `https://`.
- **TLS is always verified on `https://`.** The `verify_tls` parameter was removed; certificate chain and hostname must match or the call fails.
- SNI set before the handshake.
- Never throws; returns `{ status, body, error }`.

### `WebSocketClient` — `src/net/websocket_client.{h,cpp}`

Asynchronous WebSocket client built on Boost.Beast + Boost.Asio.

- Dedicated `io_context` thread.
- **`set_bearer_token()`** injects `Authorization: Bearer <token>` into the handshake via a Beast decorator.
- **No application-level read deadline.** Liveness is owned by Beast's own `stream_base::timeout` (`idle_timeout = 90s`, `keep_alive_pings = true`).
- **Exponential backoff with ±20% jitter** drawn from `std::mt19937`. A fleet of thousands does not reconnect in lockstep.
- **Close codes interpreted.** `1000`, `1001`, `1009`, `1012`, `1013` retry. `1008` (Policy Violation) stops permanently. `1011` retries (server bug).
- **Graceful close on `stop()`**: sends a `1000 Normal` close frame (with 2s timeouts) before tearing down, so the server logs a clean close instead of a `1006 abnormal closure`.
- **TLS is always verified on `wss://`** — no opt-out.
- Send/stop race serialized by `lifecycle_mutex`.

### `OutboundQueue` — `src/net/outbound_queue.{h,cpp}`

Bounded, priority-ordered FIFO. Four priorities. Bounded by both message count and total bytes.

### `MessageRouter` — `src/net/message_router.{h,cpp}`

Type-keyed dispatch for incoming JSON. Handlers registered by string type. Unknown types logged and ignored.

---

## 11. Utility Layer

- `glob_match` — `*`, `?`, directory-prefix matching.
- `file_hash` — streaming SHA-256 via OpenSSL EVP.
- `base64` — RFC 4648 encoding via OpenSSL EVP.

---

## 12. Storage Layer

One SQLite file. Three tables.

```
data/agent.db
  ├── files      — one row per file (metadata + hash + scan state)
  ├── state      — key/value for agent state (agent_id, ...)
  └── walks      — reserved for scan history (empty)
```

### `files` table

Identity (`id`, `dev`, `inode`), path decomposition, POSIX metadata, flags, classification (`category`, `file_type`), scan result (`is_sensitive`, `sensitivity`, `scan_status`), scan metadata (`content_hash`, `last_scanned_at`, `scan_attempts`, `scan_error`), bookkeeping (`first_seen`, `last_seen`, `walk_id`).

**Unique constraint:** `(dev, inode)`.

**Indexes:** `path`, `dirname`, `extension`, `category`, `mtime_ns DESC`, `size DESC`, `uid`, `walk_id`, `is_sensitive` (partial), `scan_status`, `content_hash`.

### Schema Versioning

`PRAGMA user_version` tracks the version. Current: **4**. Migrations apply automatically on startup.

### `Database` / `FileStore` / `StateStore`

- `Database` is a thin SQLite wrapper with WAL, `synchronous=NORMAL`, `foreign_keys=ON`, `busy_timeout=5000`. No logging in `close()` — the destructor may run after logger teardown.
- `FileStore` is the domain wrapper for `files`: upsert, pending-scan queries, verdict deduplication, scan-result writes, error recording, and aggregations.
- `StateStore` is the key/value store backed by `state`. Used by `agent_sync` for `agent_id`.

---

## 13. Feature Modules

### `filesystem_watcher`

Real-time filesystem events via efsw.

- 300 ms trailing-edge debounce.
- Priority-based merge: `modified (4) > created (3) > moved (2) > deleted (1)`.
- Two-tier exclusion (glob + directory prefix).
- Self-exclusion of the agent's own log and data directories (fixes the log feedback loop).
- Async watcher setup on a dedicated thread.
- Three threads total: main service, efsw, flusher (50 ms tick).

### `inventory`

Periodic full filesystem scan.

- Scheduled on an interval (default 900s), owns its worker thread.
- Rich metadata capture: path decomposition, timestamps in nanoseconds, POSIX permissions, file type, category.
- SHA-256 hashing for eligible files.
- Explicit `scan_status` assignment: `pending`, `not_applicable`, `skipped_too_large`.
- Batched inserts every 5,000 rows.
- Graceful stop within ~20 ms; partial commits on cancel.

### `event_logger`

Subscribes to all events on the bus and logs them. Pure observability.

### `websocket_transport`

Owns the WebSocket connection to the Core server.

- Reads `server.base_url` and `server.api_token` from the top-level `server` config section.
- Derives the WebSocket URL from `server.base_url` unless an explicit `server_url` override is set.
- Waits for `agent_id` in the state store before connecting.
- On graceful shutdown (`UserRequested`, `FatalError`, `ConfigError`) sends `{"type":"agent_shutdown","payload":{"reason":...}}` with `Priority::Critical` **before** closing the socket.
- Handles server-pushed `token_revoked` and `agent_revoked` messages by calling `ShutdownController::request(AuthRevoked/AgentRevoked)`.
- Default handlers for `ping`, `policy_update`, `command`.

### `agent_sync`

The HTTP channel between the agent and the Core server.

- **Enrollment** — `POST /api/v1/agent/register` (once at startup).
- **Heartbeat** — `POST /api/v1/agent/heartbeat`, default 60 s.
- On startup with a saved `agent_id`, sends **one immediate validation heartbeat** so a revoked token is detected within milliseconds instead of after the first interval.
- 401 → `ShutdownController::request(AuthRevoked)` with the message `"no valid API token — heartbeat rejected (401) — token is invalid, expired, or revoked"`.
- 403 → `ShutdownController::request(AgentRevoked)`.
- 404 → clears `agent_id` and re-enrolls.
- Empty `server.api_token` at init → error message says **"no valid API token"** explicitly.
- Interruptible sleep; also wakes when the shutdown controller is triggered.

### `content_scanner`

Reads text files, splits into overlapping chunks, sends to Core, writes verdicts back.

- Reads from `server.base_url` and `server.api_token`.
- Extension filter, size filter, existence check, SHA-256 of content, verdict deduplication by hash.
- Chunk size 1 MB, overlap 256 bytes.
- Batches respect `batch_size` and `batch_max_bytes`.
- Updates `is_sensitive`, `sensitivity`, `scan_status = 'scanned'` on success.
- Content is never persisted; read into memory, sent, discarded.

---

## 14. TLS / OpenSSL

### Why

HTTP and WebSocket traffic carries the `axdlp_user_...` token. If that token leaks, an attacker can impersonate the agent.

### How

- OpenSSL 3.3.2 built from source for all four targets by `scripts/build-openssl.sh`.
- All trees are static (`no-shared`); the agent links `libssl.a` and `libcrypto.a` directly.
- `ldd` shows no `libssl.so` dependency — baked in.

### Verification

Both `HttpClient` and `WebSocketClient` verify chain **and** hostname. Implemented via `X509_VERIFY_PARAM_set1_host()` because Boost's `host_name_verification` doesn't exist in Boost 1.90.

### No Opt-Out

`verify_tls` was removed from every layer:

- `TransportModuleConfig`, `AgentSyncModuleConfig` — field removed.
- `HttpClient::post_json` — parameter removed.
- `WebSocketClient::set_verify_tls` — method removed.
- `config/agent.schema.json` — property removed.
- Legacy `verify_tls` keys in config files produce a **warning that they are ignored**; the agent still verifies.

### SNI

Both clients call `SSL_set_tlsext_host_name()` before the handshake.

---

## 15. Configuration System

### Top-Level `server` Section

```json
"server": {
  "base_url": "http://127.0.0.1:3000",
  "api_token": "axdlp_user_...",
  "heartbeat_interval_seconds": 60,
  "reconnect_backoff_seconds": 10,
  "reconnect_backoff_max_seconds": 300
}
```

### Module Sections

- `filesystem_watcher`: watch paths, exclude paths, debounce.
- `inventory`: watch paths, exclude paths, interval, hash settings, scannable categories.
- `transport`: `enabled`, optional `server_url` override.
- `agent_sync`: `enabled`.
- `content_scanner`: batch size, chunking, retries, text extensions.

### Fail-Safe Behavior

Missing file, invalid JSON, wrong type, invalid value — all produce warnings and fall back to safe defaults. **The agent always starts.** Config problems never prevent operation.

---

## 16. The Data Model

After an inventory, `files` contains every regular file under the watch paths. Fields are populated by `stat()` during the walk, by SHA-256 hashing for eligible files, and by the content scanner for verdicts.

**Data flow:**

```
1. Inventory writes metadata + hash, scan_status = 'pending'
2. Content scanner queries pending files, reads content, verifies hash
3. Batches chunks to POST /api/v1/agent/scan/batch
4. Server runs PII detection and returns per-file verdicts
5. Agent writes is_sensitive / sensitivity / scan_status = 'scanned'
6. Enforcement (future) acts on is_sensitive = 1 files
```

---

## 17. WebSocket Lifecycle (Agent ↔ Server)

This is the newest and most critical part of the system. Read it carefully.

### Connection Setup

1. `agent_sync` enrolls via HTTP, gets `agent_id`, writes it to SQLite `state`.
2. `transport` polls the state store until `agent_id` appears.
3. `transport` derives the WebSocket URL from `server.base_url` (or uses an explicit override).
4. `WebSocketClient::start()` spawns the I/O thread.
5. Handshake carries `Authorization: Bearer <token>`.
6. On 101 Switching Protocols, the client transitions to `Connected`.
7. Every 30 s the server pings; Beast on the client responds with pongs automatically.
8. Every 60 s `agent_sync` heartbeats over HTTP, updating `last_seen_at`.

### Message Flow

- **Server → agent**: any handler can call `agentHub.SendToAgent(id, msg)` or `agentHub.BroadcastToOrg(orgID, msg)`.
- **Agent → server**: any module can call `TransportModule::send_message(json)`. The message is queued in `OutboundQueue` and written by the write pump.
- **Goodbye on graceful shutdown**: `TransportModule::stop()` sends `{"type":"agent_shutdown","payload":{"reason":...}}` at `Priority::Critical` before closing.

### Reconnect Policy

Exponential backoff, ±20% jitter, base 10 s, cap 300 s.

```
reconnecting in 11s (base=10s, max=300s)
reconnecting in 21s (base=20s, max=300s)
reconnecting in 42s (base=40s, max=300s)
reconnecting in 88s (base=80s, max=300s)
reconnecting in 176s (base=160s, max=300s)
reconnecting in 300s (base=300s, max=300s)   ← capped
```

### Close Code Handling

| Code | Meaning | Behavior |
|------|---------|----------|
| 1000 | Normal | Reconnect |
| 1001 | Going Away | Reconnect |
| 1006 | Abnormal | Reconnect |
| 1008 | Policy Violation | **Stop permanently** (auth revoked) |
| 1009 | Too Big | Reconnect |
| 1011 | Internal | Reconnect |
| 1012 | Service Restart | Reconnect |
| 1013 | Try Again Later | Reconnect |

### Server-Side Revocation

Two independent mechanisms:

1. **Token watcher**: the server re-checks the DB every 60 s. If the token is revoked, it sends `{"type":"token_revoked",...}` then a `1008 Policy Violation` close frame.
2. **Heartbeat 401**: the agent's next HTTP heartbeat returns 401. `agent_sync` calls `ShutdownController::request(AuthRevoked)`.

Whichever fires first, the whole agent exits cleanly in ~1 second.

### Atomic Shutdown

Every module can request shutdown at any time. The service main loop unblocks and runs `stop_all()` then `shutdown_all()` in reverse order. The final log line shows the reason.

---

## 18. The Complete Bug History

### WebSocket-Specific Bugs (newest)

**Bug W1 — 60s forced disconnect.** `start_read()` used `expires_after(60s)` on every read. Server pings don't complete application-level reads, so the deadline fired every 60 s and killed the connection. **Fix:** removed the per-read deadline; liveness is now Beast's `stream_base::timeout` with `keep_alive_pings = true`.

**Bug W2 — Handshake rejected (401) because no bearer was sent.** **Fix:** `set_bearer_token()` + a Beast decorator that injects `Authorization` into the handshake.

**Bug W3 — Slow Unregister.** When `readPump` returned, the writer waited up to 30 s for the next ping tick. **Fix:** call `cancel()` immediately after `readPump` returns.

**Bug W4 — `1006 abnormal closure` on Ctrl+C.** Client sent no close frame. **Fix:** `stop()` now sends a `1000 Normal` close frame with tightened 2 s timeouts.

**Bug W5 — Send-on-closed-channel panic.** `Unregister` closed the `Send` channel while a broadcast could still be selecting on it. **Fix:** introduced a separate `Done` channel; `Send` is never closed.

**Bug W6 — Thundering herd on reconnect.** All agents reconnected synchronously. **Fix:** ±20% jitter via `std::mt19937`.

**Bug W7 — No caps.** Unlimited connections. **Fix:** 100k global / 20k per org.

**Bug W8 — No rate limit.** One bad agent could DoS the server. **Fix:** `x/time/rate` limiter, 20/s per agent.

**Bug W9 — Slow shutdown blocked by WebSockets.** **Fix:** `CloseAll()` runs before `srv.Shutdown()`.

**Bug W10 — Close codes ignored.** Revoked agents kept reconnecting. **Fix:** `should_reconnect_on_close()`.

**Bug W11 — `write_in_flight` not reset.** **Fix:** reset on every disconnect path.

**Bug W12 — `close_code` type mismatch.** **Fix:** `static_cast<websocket::close_code>(cr.code)`.

**Bug W13 — `handshake_declined` doesn't exist in this Boost.** **Fix:** match on `ec.message()` instead.

**Bug W14 — `bugprone-unused-return-value` from clang-tidy on async operations.** **Fix:** wrap every non-void-returning Beast call in explicit `static_cast<void>(...)`.

### Configuration & Shutdown Bugs

**Bug C1 — `verify_tls` was a footgun.** Removed from every layer. Legacy keys warn and are ignored.

**Bug C2 — Duplicate token across three modules.** Consolidated into `server.api_token`.

**Bug C3 — Revoked token silently ignored while running.** `agent_sync` now requests shutdown on 401/403; the server's watcher also closes the socket with 1008.

**Bug C4 — Empty token at startup allowed a stale agent_id heartbeat.** Now the agent sends an immediate validation heartbeat and errors with **"no valid API token"** before entering the loop.

### Original 23 Bugs (kept for history)

Bugs 1–23 in the previous revision are unchanged and still fixed. Highlights: log feedback loop, shutdown segfault, dev/inode collision, SQLite cross-compile failure, CMake `file(DOWNLOAD)` silent failure, missing C language in `project()`, walker interruptibility, debounce priority, heartbeat timer advance, `std::ignore` vs `void`, `host_name_verification` in Boost 1.90, OpenSSL `lib/` vs `lib64/`, CMake 4.x `find_package(OpenSSL)` quirks, i686 OpenSSL selection, duplicated `glob_match`, post-refactor include paths, and `AgentSync` starting after `initialize()` failure.

---

## 19. Runtime Evidence

### Successful Startup (fresh token)

```
[info] AxiomDLP Agent v0.1.0
[info] Config loaded from: config/agent.default.json
[info] Database: opened 'data/agent.db'
[info] Migrations: schema version 4 -> 4
[info] ModuleManager: registering module 'filesystem_watcher' v1.0.0
[info] ModuleManager: registering module 'inventory' v1.0.0
[info] ModuleManager: registering module 'event_logger' v1.0.0
[info] ModuleManager: registering module 'transport' v1.0.0
[info] ModuleManager: registering module 'agent_sync' v1.0.0
[info] ModuleManager: registering module 'content_scanner' v1.0.0
[info] transport module: initialized (url=ws://127.0.0.1:3000/api/v1/agent/ws, backoff=10s..300s)
[info] agent_sync module: initialized (server=http://127.0.0.1:3000, heartbeat_interval=60s)
[info] axiomdlp-agent starting (foreground mode)
[info] agent_sync: POST http://127.0.0.1:3000/api/v1/agent/register (hostname=kali)
[info] agent_sync: enrolled (agent_id=1, user_id=1, org_id=2)
[info] transport module: connecting to ws://127.0.0.1:3000/api/v1/agent/ws?agent_id=1
[info] WebSocketClient: connected (ws)
[info] transport module: state = connected
[debug] agent_sync: heartbeat ok (agent_id=1)
```

### Server-Side Registration

```json
{"msg":"Agent registered","agent_id":1,"user_id":1,"org_id":2,"hostname":"kali","os":"linux","version":"0.1.0"}
{"msg":"agent hub: registered","agent_id":1,"user_id":1,"org_id":2,"token_id":1,"total":1,"org_total":1}
```

### Broadcast Push

Server:
```bash
curl -X POST /api/v1/agents/broadcast -d '{"type":"policy_update","payload":{"version":1}}'
```

Agent:
```
[info] transport: received policy_update: {"payload":{"version":1},"type":"policy_update"}
```

### Graceful Ctrl+C

```
[info] axiomdlp-agent received stop request (reason=user_requested)
[info] ModuleManager: stopping 6 module(s)
[info] transport module: sending goodbye (reason=user_requested)
[info] WebSocketClient: sent graceful close (code=1000)
[info] transport module: state = disconnected
[info] Service returned rc=0 (reason=user_requested)
[info] Application shutting down
```

Server:
```json
{"msg":"agent ws: agent shutting down gracefully","agent_id":1,"payload":{"agent_id":1,"reason":"user_requested"}}
{"msg":"agent ws: clean close","agent_id":1}
{"msg":"agent hub: unregistered","agent_id":1,"remaining":0}
```

### Revocation While Running

Server watcher:
```json
{"msg":"agent ws: token revoked, closing connection","agent_id":1,"token_id":1}
```

Or agent-side heartbeat:
```
[error] agent_sync: no valid API token — heartbeat rejected (401) — token is invalid, expired, or revoked
[info] axiomdlp-agent received stop request (reason=auth_revoked)
[info] transport module: sending goodbye (reason=auth_revoked)
[info] WebSocketClient: sent graceful close (code=1000)
[info] Application shutting down
```

### Revoked Token at Startup

```
[info] agent_sync: validating token with immediate heartbeat
[error] agent_sync: no valid API token — heartbeat rejected (401) — token is invalid, expired, or revoked
[info] axiomdlp-agent received stop request (reason=auth_revoked)
[info] Application shutting down
```

Exits in ~1 s. No reconnect loop.

---

## 20. Known Limitations

### 20.1 ModuleManager Doesn't Skip Failed Modules

A module that fails `initialize()` still gets `start()` called on it. Fix pending.

### 20.2 No Process or User Attribution

Events carry a path but not the process. Requires fanotify (Linux) or Minifilter (Windows).

### 20.3 inotify Queue Overflow

The periodic inventory scan catches anything the watcher misses.

### 20.4 No Network Filesystem Support

inotify doesn't fire on NFS/SMB.

### 20.5 Windows Ctrl+C Not Handled in Foreground

No `SetConsoleCtrlHandler`. SCM path works.

### 20.6 Windows Path Case Sensitivity

The glob matcher is case-sensitive.

### 20.7 Dev/Inode Are Placeholders

`dev` is always 0; `inode` is a path hash.

### 20.8 Content Scanner Only Handles Text Files

PDFs, DOCX, XLSX, images are marked `not_applicable`.

### 20.9 `walks` Table Unused

The schema exists; nothing populates it.

### 20.10 Mixed Path Separators on Windows

Cosmetic only.

### 20.11 OpenSSL Build Requires ~20 Minutes

One-time. Cached trees reuse.

---

## 21. What Does Not Exist Yet

| Feature | Notes |
|---------|-------|
| Policy engine | Scan results → decisions |
| User-mode enforcement | Block file operations |
| Kernel enforcement | Minifilter / eBPF / fanotify |
| IPC + tray helper | Notifications, pause/resume |
| Auto-update | Signed manifests, atomic swap |
| Code signing | Authenticode / GPG |
| Packaging | `.deb`, `.rpm`, MSI |
| Rich document extraction | PDF, DOCX, XLSX inside content scanner |
| Process/user attribution | Requires kernel-level monitoring |

---

## 22. Lessons Learned

### On Concurrency

- A single `ShutdownController` beats scattered flags. One authority, one reason, one path.
- `Done` channels are safer than closing `Send` channels under concurrent send.
- Jitter is mandatory for fleet-scale reconnection.
- Bounded queues prevent network partitions from becoming OOM.

### On WebSocket

- No per-read deadlines. The protocol owns liveness via pings.
- Always send a graceful close frame on shutdown.
- Interpret close codes. Not every disconnect should retry.
- Separate concerns: `agent_sync` (HTTP) and `transport` (WS) are different lifecycles.

### On Configuration

- One source of truth per concept. `server.api_token` beats three copies.
- Security defaults must not be options. `verify_tls` is gone.
- Backwards compatibility is cheap when done in the parser.

### On Cross-Platform

- Four targets from one host is achievable but requires discipline.
- Toolchain isolation prevents host libraries from leaking.
- Static dependencies eliminate a class of deployment problems.

### On Debugging

- Read the actual error. `close 1006` and `handshake declined` are symptoms, not causes.
- Log at the right level. Heartbeat success at DEBUG; revoke at ERROR.
- Run for long enough to see periodic behavior — 60 s+ tests reveal timing bugs.

### On the Project

- The infrastructure (build, service, logging, config, event bus, TLS, shutdown) is genuinely reusable.
- The DLP logic is the easy part once the infrastructure is in place.
- Ship small. Every change since the beginning has been verified before the next.
- Bug histories like this document are worth their weight. They explain *why* the code looks the way it does.

---

## Summary

The AxiomDLP Endpoint Agent is a **cross-platform, TLS-capable, hashing-enabled, content-scanning, atomically-shutdownable service foundation** with:

- **Four verified build targets** — Linux x86_64/i686, Windows x86_64/i686.
- **Six working modules** — filesystem_watcher, inventory, event_logger, websocket_transport, agent_sync, content_scanner.
- **Atomic shutdown** — one authority, typed reasons, end-to-end logging.
- **Bidirectional WebSocket** — server push, agent push, graceful goodbye, jittered reconnect, close-code interpretation.
- **Proactive revocation** — server watcher + agent heartbeat both detect revoked tokens within 60 s and stop the agent cleanly.
- **TLS everywhere** — unconditional verification on `https://` and `wss://`.
- **Consolidated config** — one top-level `server` section.
- **Static OpenSSL 3.3.2** — no runtime `.so` dependencies.
- **SHA-256 fingerprinting** — every eligible file.
- **Content scanner** — chunks text files, sends to Core, writes verdicts back.
- **A clean layered source tree** — foundation, events, modules, platform, net, util, storage, features.
- **A complete bug history** — every issue found, diagnosed, and fixed.

**What it does today:** watches the filesystem, maintains a complete inventory, hashes every eligible file, scans text files for PII via the Core, maintains a persistent TLS WebSocket with the Core, handles server-initiated revocation, and shuts down cleanly whenever asked — by the user, by a fatal error, or by the server.

**What it will do:** enforce policy, block sensitive data movement, coordinate with a fleet, and provide user-facing notifications.

The pipeline is complete. The decision and enforcement layers are the next milestone.
