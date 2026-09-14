# AxiomDLP Endpoint Agent — Project Structure Explained

> A complete reference for every folder and file in the repository. This document explains **what each directory is for**, **what each file contains**, and **how they fit together**.

---

## Table of Contents

- [AxiomDLP Endpoint Agent — Project Structure Explained](#axiomdlp-endpoint-agent--project-structure-explained)
  - [Table of Contents](#table-of-contents)
  - [1. Root Files](#1-root-files)
  - [2. `cmake/` — Build Helpers](#2-cmake--build-helpers)
  - [3. `external/` — Third-Party Dependencies](#3-external--third-party-dependencies)
  - [4. `src/` — Source Code](#4-src--source-code)
    - [4.1 `src/core/` — Framework](#41-srccore--framework)
    - [4.2 `src/interfaces/` — Contracts](#42-srcinterfaces--contracts)
    - [4.3 `src/event/` — Event Model](#43-srcevent--event-model)
    - [4.4 `src/policy/` — Policy Engine](#44-srcpolicy--policy-engine)
    - [4.5 `src/scanner/` — Content Scanning](#45-srcscanner--content-scanning)
    - [4.6 `src/enforcement/` — Enforcement](#46-srcenforcement--enforcement)
    - [4.7 `src/storage/` — Local Persistence](#47-srcstorage--local-persistence)
    - [4.8 `src/transport/` — Connection Layer](#48-srctransport--connection-layer)
    - [4.9 `src/ipc/` — Service ↔ Helper IPC](#49-srcipc--service--helper-ipc)
    - [4.10 `src/telemetry/` — Metrics \& Health](#410-srctelemetry--metrics--health)
    - [4.11 `src/platform/` — OS Abstraction](#411-srcplatform--os-abstraction)
      - [`platform/windows/`](#platformwindows)
      - [`platform/linux/`](#platformlinux)
    - [4.12 `src/modules/` — Feature Modules](#412-srcmodules--feature-modules)
  - [5. `kernel/` — Future Kernel Drivers](#5-kernel--future-kernel-drivers)
  - [6. `helper/` — User Session Helper](#6-helper--user-session-helper)
  - [7. `config/` — Configuration Files](#7-config--configuration-files)
  - [8. `tests/` — Test Suite](#8-tests--test-suite)
  - [9. `scripts/` — Developer \& Build Scripts](#9-scripts--developer--build-scripts)
  - [10. `packaging/` — Installers \& Packages](#10-packaging--installers--packages)
  - [11. `docs/` — Documentation](#11-docs--documentation)
  - [How the Pieces Fit Together (Quick Recap)](#how-the-pieces-fit-together-quick-recap)
  - [Summary Table](#summary-table)

---

## 1. Root Files

| File | Purpose |
|------|---------|
| **`CMakeLists.txt`** | Root CMake build file. Detects platform, defines global compiler flags, includes subdirectories, links libraries, and produces the final agent binary. |
| **`CMakePresets.json`** | Predefined build configurations (`debug`, `release`, `relwithdebinfo`). Lets developers run `cmake --preset=debug` instead of typing long flags. |
| **`README.md`** | Top-level project introduction, architecture overview, quick-start instructions. |
| **`LICENSE`** | Apache 2.0 license text. |
| **`.gitignore`** | Excludes build artifacts, IDE files, logs, and secrets from Git. |
| **`.clang-format`** | Code formatting rules. Run `clang-format -i` on all sources before committing. |
| **`.clang-tidy`** | Static analysis rules. CI runs this; developers run it locally too. |

---

## 2. `cmake/` — Build Helpers

Contains custom CMake modules that make the root `CMakeLists.txt` cleaner.

| File | Purpose |
|------|---------|
| **`FindLibcurl.cmake`** | Locates libcurl on Linux/Windows if the system doesn't provide one. |
| **`FindSQLite3.cmake`** | Locates SQLite3 headers and libraries. |
| **`FindOpenSSL.cmake`** | Locates OpenSSL for TLS and crypto. |
| **`FindDPAPI.cmake`** | Windows-only. Detects DPAPI availability for secure storage. |
| **`FindLibsecret.cmake`** | Linux-only. Detects libsecret for keyring access. |
| **`CompilerWarnings.cmake`** | Standard warning flags per compiler (MSVC/GCC/Clang). Imported once, applied to all targets. |
| **`Packaging.cmake`** | Helper macros for CPack, DEB, RPM, MSI generation. |

**Why this exists:** Keeps platform-specific build logic out of individual modules. Every module just says "I need libcurl" and the build system figures it out.

---

## 3. `external/` — Third-Party Dependencies

Where third-party libraries live. Managed via git submodules or CMake FetchContent.

| Folder | Library | Purpose |
|--------|---------|---------|
| `nlohmann_json/` | nlohmann/json | JSON parsing and serialization |
| `spdlog/` | spdlog | Structured, fast, thread-safe logging |
| `fmt/` | fmt | Modern string formatting (used by spdlog) |
| `sqlite3/` | SQLite3 | Local embedded database |
| `libcurl/` | libcurl | HTTP/HTTPS client (used later) |
| `openssl/` | OpenSSL | TLS, certificates, hashing |
| `boost/` | Boost (Asio + Beast) | WebSocket and async I/O (later) |
| `catch2/` | Catch2 | Unit testing framework |

**Why this exists:** All external code is pinned to a specific version and vendored into the repo. This makes builds reproducible and offline-friendly.

---

## 4. `src/` — Source Code

The main source tree. Every subsystem lives under `src/`.

### 4.1 `src/core/` — Framework

The backbone of the agent. **Platform-agnostic** — no OS headers here.

| File | Purpose |
|------|---------|
| **`application.{h,cpp}`** | The top-level orchestrator. Boots the agent: loads config, creates `ServiceContext`, initializes the module manager, starts modules, handles shutdown. |
| **`module_manager.{h,cpp}`** | Registers, initializes, starts, stops, and supervises all modules. Also reports aggregate health. |
| **`event_bus.{h,cpp}`** | In-process publish/subscribe. Modules publish events; subscribers (policy engine, storage, telemetry) receive them. |
| **`service_context.{h,cpp}`** | The dependency container. Holds references to config, logger, event bus, storage, transport, module manager. Passed into every module on init. |
| **`config.{h,cpp}`** | Loads and validates `agent.default.json` and overrides. Provides typed access to config values. |
| **`logger.{h,cpp}`** | Thin wrapper around spdlog. Sets up log sinks (console, file, rotating). Enforces redaction rules. |
| **`version.{h,cpp}`** | Compile-time and runtime version strings. Used in logs, heartbeat, and packaging. |
| **`error.{h,cpp}`** | Standard error codes and a `Result<T>` type. Used across module boundaries instead of exceptions. |

**Why this exists:** The core is the framework. It has no idea what a filesystem or USB is. It just knows how to run modules.

---

### 4.2 `src/interfaces/` — Contracts

Pure abstract base classes. No implementation. Every subsystem agrees on these.

| File | Purpose |
|------|---------|
| **`i_module.h`** | The module contract: `initialize`, `start`, `stop`, `shutdown`, `health`. |
| **`i_event_source.h`** | For modules that produce events (filesystem, USB, clipboard). Defines the callback registration. |
| **`i_enforcer.h`** | For modules that can block operations. Defines `enforce(Decision, Event)`. |
| **`i_policy_provider.h`** | For sources of policies (local cache, remote sync, bootstrap). |
| **`i_transport.h`** | Abstraction for HTTP/WebSocket. Lets you swap real transport for a mock in tests. |
| **`i_storage.h`** | Abstraction for local persistence. |
| **`i_ipc.h`** | Abstraction for IPC. Windows named pipes and Unix sockets implement this. |
| **`i_crypto.h`** | Abstraction for encryption and secure storage. DPAPI and libsecret implement this. |

**Why this exists:** Interfaces decouple. A module written against `IStorage` doesn't care if storage is SQLite, in-memory, or a mock.

---

### 4.3 `src/event/` — Event Model

The data model for everything that flows through the agent.

| File | Purpose |
|------|---------|
| **`event.h`** | The `Event` struct — the core data type. Contains ID, type, timestamp, user, process, subject, payload, metadata. |
| **`event.cpp`** | Constructor helpers, validation, hashing. |
| **`event_types.h`** | Enumerations and string constants for all event types (`file_access`, `usb_insert`, etc.). Single source of truth. |
| **`event_factory.{h,cpp}`** | Factory helpers to construct events with correct defaults per type. |
| **`event_serializer.{h,cpp}`** | JSON serialization/deserialization. Later switchable to Protobuf. |

**Why this exists:** Every part of the agent speaks the same event language. Adding a new event type means editing `event_types.h` — nothing else.

---

### 4.4 `src/policy/` — Policy Engine

The brain. Turns events + scan results into decisions.

| File | Purpose |
|------|---------|
| **`policy.h`** | The `Policy` struct: id, name, entity type, threshold, action, priority, enabled, scope. |
| **`policy.cpp`** | Parsing, validation, comparison. |
| **`policy_engine.{h,cpp}`** | The evaluator. Receives events, evaluates against applicable policies, produces a `Decision`. |
| **`policy_cache.{h,cpp}`** | Local cache of policies in SQLite. Loaded at startup. Reloaded on sync. |
| **`policy_sync.{h,cpp}`** | Fetches policies from core. Stub for now; full implementation later. |
| **`decision.{h,cpp}`** | The `Decision` struct: action, risk score, reason, matching policy ID, enforce-sync flag. |

**Why this exists:** Policy logic is pure and testable. It doesn't know about files, USB, or HTTP. It only knows events and policies.

---

### 4.5 `src/scanner/` — Content Scanning

Turns raw content into detections.

| File | Purpose |
|------|---------|
| **`scanner.h`** | The unified `ScanResult` type and the `IScanner` interface. |
| **`scanner.cpp`** | Orchestrates multiple scanner backends. Decides local vs remote. |
| **`regex_scanner.{h,cpp}`** | Fast, offline pattern matching for credit cards, SSNs, emails, phone numbers. |
| **`local_pii_scanner.{h,cpp}`** | Optional embedded ML model for names, addresses. Placeholder for now. |
| **`remote_pii_scanner.{h,cpp}`** | Calls the core's `/pii/detect` endpoint when online. |

**Why this exists:** Policy engine shouldn't care *how* content was scanned. It just receives a unified `ScanResult`. Swapping regex for ML later is trivial.

---

### 4.6 `src/enforcement/` — Enforcement

Turns decisions into actions.

| File | Purpose |
|------|---------|
| **`enforcer.h`** | The `Enforcer` interface. Accepts a `Decision` + `Event`, returns success/failure. |
| **`enforcer.cpp`** | Dispatches to the correct backend based on event type. |
| **`enforcer_registry.{h,cpp}`** | Maps event types → enforcer backends. Allows multiple backends per type. |
| **`backends/usermode/`** | User-mode enforcement implementations (initially empty; filled when needed). |
| **`backends/kernel/`** | Kernel-mode enforcement (empty until kernel drivers exist). |

**Why this exists:** Enforcement is the most platform-specific part. Isolating it behind a registry means kernel drivers slot in later without touching business logic.

---

### 4.7 `src/storage/` — Local Persistence

Everything that touches the disk.

| File | Purpose |
|------|---------|
| **`database.{h,cpp}`** | SQLite wrapper. Connection management, prepared statements, transactions. |
| **`migrations.{h,cpp}`** | Schema versioning. Runs on startup to bring DB up to date. |
| **`event_queue.{h,cpp}`** | Persistent queue of events waiting to be uploaded. Survives crashes. |
| **`secure_store.{h,cpp}`** | Secure token/key storage. Uses DPAPI on Windows, libsecret on Linux. |

**Why this exists:** Offline-first means local state is critical. SQLite is transactional, queryable, and battle-tested.

---

### 4.8 `src/transport/` — Connection Layer

Communication with the core. Stubbed for Phase 1.

| File | Purpose |
|------|---------|
| **`transport_manager.{h,cpp}`** | Top-level orchestrator: heartbeat loop, reconnect loop, message routing. |
| **`http_client.{h,cpp}`** | libcurl-based HTTPS client. Used for enrollment, heartbeat, policy fetch, event upload. |
| **`ws_client.{h,cpp}`** | WebSocket client (Boost.Beast). Used for the push channel. Stub for now. |
| **`heartbeat.{h,cpp}`** | Periodic heartbeat loop with retry and backoff. |
| **`reconnect.{h,cpp}`** | Exponential backoff with jitter. Shared by HTTP and WebSocket. |
| **`protocol.{h,cpp}`** | Message schemas for wire format. JSON now, Protobuf later. |

**Why this exists:** Isolates all networking code. When you swap JSON for Protobuf, only `protocol` changes.

---

### 4.9 `src/ipc/` — Service ↔ Helper IPC

Communication between the privileged service and the user-session helper.

| File | Purpose |
|------|---------|
| **`ipc_server.{h,cpp}`** | Runs in the service. Listens for helper connections (named pipe / Unix socket). |
| **`ipc_client.{h,cpp}`** | Runs in the helper. Connects to the service. |
| **`messages.h`** | Message types and payload schemas. Versioned. |

**Why this exists:** The service runs as SYSTEM/root (no GUI). The helper runs as the user. IPC is the only bridge.

---

### 4.10 `src/telemetry/` — Metrics & Health

Internal observability.

| File | Purpose |
|------|---------|
| **`metrics.{h,cpp}`** | Counters, gauges, histograms for events, scans, blocks, queue size, latency. |
| **`health.{h,cpp}`** | Per-module health reporting and aggregate agent health. |

**Why this exists:** You cannot operate a fleet of agents without visibility. Health and metrics feed heartbeats and local diagnostics.

---

### 4.11 `src/platform/` — OS Abstraction

All OS-specific code lives here, behind interfaces from `src/interfaces/`.

#### `platform/windows/`

| File | Purpose |
|------|---------|
| **`service.{h,cpp}`** | SCM integration: `ServiceMain`, control handler, install/uninstall. |
| **`named_pipe.{h,cpp}`** | Windows implementation of `IIpc`. |
| **`dpapi.{h,cpp}`** | Windows implementation of `ICrypto`. Encrypts secrets with DPAPI. |
| **`registry.{h,cpp}`** | Reads/writes agent config from the Windows Registry. |
| **`process.{h,cpp}`** | Process attribution: which user, which process triggered an event. |
| **`file_watcher.{h,cpp}`** | File event capture (`ReadDirectoryChangesW`, later Minifilter). |
| **`clipboard.{h,cpp}`** | Clipboard hooks. |
| **`usb.{h,cpp}`** | USB device events (`WM_DEVICECHANGE`, SetupAPI). |
| **`printer.{h,cpp}`** | Print spooler monitoring. |

#### `platform/linux/`

| File | Purpose |
|------|---------|
| **`systemd.{h,cpp}`** | systemd integration: notify, signals, service readiness. |
| **`unix_socket.{h,cpp}`** | Linux implementation of `IIpc`. |
| **`keyring.{h,cpp}`** | Linux implementation of `ICrypto`. Uses libsecret / kernel keyring. |
| **`process.{h,cpp}`** | Process attribution via `/proc`, later eBPF. |
| **`file_watcher.{h,cpp}`** | File event capture (`inotify`, later `fanotify`). |
| **`clipboard.{h,cpp}`** | X11 / Wayland clipboard monitoring. |
| **`usb.{h,cpp}`** | USB device events via `udev`. |
| **`printer.{h,cpp}`** | CUPS monitoring. |

**Why this exists:** Business logic never includes `<windows.h>` or `<sys/inotify.h>`. Only files here do.

---

### 4.12 `src/modules/` — Feature Modules

Each module is a self-contained DLP capability. They live under `modules/`.

| File / Folder | Purpose |
|---------------|---------|
| **`module_base.{h,cpp}`** | Common base class implementing boilerplate for `IModule`. Modules inherit from this. |
| **`filesystem/`** | Filesystem module: watches file access, scans content, blocks if policy says so. Both `IEventSource` and `IEnforcer`. |
| **`usb/`** | USB module: detects USB insert/remove, scans copied content. |
| **`clipboard/`** | Clipboard module: monitors clipboard writes, scans text, blocks on policy. |
| **`printer/`** | Print module: monitors print jobs, scans print content. |
| **`browser/`** | Browser module: placeholder for native messaging bridge to a browser extension. |
| **`process/`** | Process module: monitors process creation, tool attribution. |

Each module folder contains:

- `*_module.h` / `*_module.cpp` — the module implementation
- `CMakeLists.txt` — builds the module as a static (or shared) library

**Why this exists:** Adding a new feature is adding a new folder here and registering it in one place. The core stays untouched.

---

## 5. `kernel/` — Future Kernel Drivers

Empty for now. Reserved for kernel-mode enforcement.

| Path | Purpose |
|------|---------|
| **`windows/minifilter/`** | Windows Minifilter driver for file system enforcement (blocking reads/writes at the syscall layer). |
| **`linux/ebpf/`** | Linux eBPF LSM programs for syscall-level enforcement. |

**Why this exists:** Kernel enforcement is the ultimate goal. This folder reserves the namespace so the architecture is visible from day one.

---

## 6. `helper/` — User Session Helper

The tray/UI process running in the user's session.

| File | Purpose |
|------|---------|
| **`CMakeLists.txt`** | Builds the helper executable. |
| **`src/main.cpp`** | Entry point. Connects to the service over IPC, initializes tray. |
| **`src/tray.cpp`** | System tray icon, menu, status indicator. |
| **`src/notifications.cpp`** | User-facing notifications (blocked event, agent paused, etc.). |
| **`src/ipc_client.cpp`** | Client side of the IPC protocol. |
| **`resources/`** | Icons, localization strings, manifest. |

**Why this exists:** The service is headless. The helper is the user's window into what the agent is doing.

---

## 7. `config/` — Configuration Files

| File | Purpose |
|------|---------|
| **`agent.default.json`** | Default configuration shipped with the agent. Baseline values for every setting. |
| **`agent.schema.json`** | JSON Schema for validating config files. Catches typos and invalid values at load time. |
| **`logging.json`** | Logging configuration: sinks, levels, rotation policy, redaction rules. |

**Why this exists:** Config is externalized. Operators can tweak behavior without recompiling.

---

## 8. `tests/` — Test Suite

| Folder | Purpose |
|--------|---------|
| **`CMakeLists.txt`** | Registers tests with CTest. |
| **`unit/core/`** | Unit tests for `src/core/` — event bus, module manager, config, logger. |
| **`unit/policy/`** | Unit tests for the policy engine and decision logic. |
| **`unit/scanner/`** | Unit tests for regex and other scanners. |
| **`integration/`** | End-to-end tests: event → scan → decide → enforce → store. Uses mocks for transport. |
| **`mocks/`** | Mock implementations of interfaces (transport, event source, enforcer) for testing. |

**Why this exists:** Every module can be tested in isolation. Integration tests catch wiring issues.

---

## 9. `scripts/` — Developer & Build Scripts

| File | Purpose |
|------|---------|
| **`build.sh`** | Linux/macOS build helper. Wraps CMake presets. |
| **`build.ps1`** | Windows build helper. Same as above but PowerShell. |
| **`package_deb.sh`** | Builds a `.deb` package for Debian/Ubuntu. |
| **`package_rpm.sh`** | Builds an `.rpm` package for RHEL/Fedora. |
| **`package_msi.ps1`** | Builds an MSI installer via WiX. |
| **`dev_mock_core.py`** | A tiny Python HTTP server that mimics the core's agent endpoints. Lets you develop the transport layer without a full core. |

**Why this exists:** Common operations are one command. New contributors don't have to learn the full build pipeline.

---

## 10. `packaging/` — Installers & Packages

| Path | Purpose |
|------|---------|
| **`linux/systemd/axiomdlp-agent.service`** | The systemd unit file for the agent. |
| **`linux/debian/control`** | Debian package metadata (name, version, dependencies, description). |
| **`linux/rpm/axiomdlp-agent.spec`** | RPM spec file. |
| **`windows/installer.wxs`** | WiX Toolset XML defining the Windows installer. |
| **`windows/service_install.ps1`** | PowerShell script to register the agent as a Windows Service. |

**Why this exists:** Packaging is OS-specific, versioned, and often maintained by a different person than the code. Keeping it separate avoids clutter.

---

## 11. `docs/` — Documentation

| File | Purpose |
|------|---------|
| **`architecture.md`** | Deep-dive on the architecture: pipeline, module system, event bus. |
| **`modules.md`** | Guide to writing new modules: contract, roles, registration, testing. |
| **`policy_format.md`** | Exact JSON schema for policies and decision model. |
| **`event_schema.md`** | Exact JSON schema for events and event types. |
| **`ipc_protocol.md`** | IPC message format, versioning, and security rules. |

**Why this exists:** Documentation lives next to the code. New contributors read these before opening `src/`.

---

## How the Pieces Fit Together (Quick Recap)

```
main.cpp
   │
   ▼
Application
   ├── loads Config
   ├── creates Logger
   ├── creates ServiceContext
   │       ├── EventBus
   │       ├── Storage
   │       ├── Transport (stub)
   │       ├── PolicyEngine
   │       ├── Scanner
   │       └── EnforcerRegistry
   │
   ├── ModuleManager
   │       ├── FilesystemModule   ─┐
   │       ├── UsbModule           │  all emit events
   │       ├── ClipboardModule     │  onto the EventBus
   │       ├── PrinterModule       │
   │       └── ...                ─┘
   │
   ├── IPCServer  ←→  Helper (tray UI)
   │
   └── starts everything, waits for SIGTERM
```

Pipeline at runtime:

```
Module emits Event
   → EventBus publishes
      → Scanner consumes, produces ScanResult
         → PolicyEngine consumes, produces Decision
            → Enforcer applies Decision
               → EventQueue persists outcome
                  → Transport uploads (when online)
```

---

## Summary Table

| Folder | One-Line Purpose |
|--------|------------------|
| `cmake/` | Build system helpers and dependency finders |
| `external/` | Vendored third-party libraries |
| `src/core/` | Framework: app, modules, event bus, config, logging |
| `src/interfaces/` | Pure abstract contracts |
| `src/event/` | Event data model and serialization |
| `src/policy/` | Policy model, engine, cache, decisions |
| `src/scanner/` | Content scanning (regex, ML, remote) |
| `src/enforcement/` | Enforcement orchestration and backends |
| `src/storage/` | SQLite, migrations, event queue, secure store |
| `src/transport/` | Connection to core (HTTP/WebSocket) |
| `src/ipc/` | Service ↔ helper communication |
| `src/telemetry/` | Metrics and health |
| `src/platform/` | OS-specific implementations (Windows/Linux) |
| `src/modules/` | Feature modules (filesystem, USB, clipboard, etc.) |
| `kernel/` | Future kernel drivers (minifilter, eBPF) |
| `helper/` | User-session tray/UI process |
| `config/` | Default configs and JSON schemas |
| `tests/` | Unit, integration, mocks |
| `scripts/` | Build, packaging, dev tools |
| `packaging/` | Installers for Linux and Windows |
| `docs/` | Deep-dive documentation |

---

