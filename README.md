# AxiomDLP Endpoint Agent

> A modular, cross-platform Data Leak Prevention (DLP) endpoint agent for Linux and Windows, written in modern C++.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Design Philosophy](#2-design-philosophy)
3. [The Five Pipeline Stages](#3-the-five-pipeline-stages)
4. [High-Level Architecture](#4-high-level-architecture)
5. [Project Structure](#5-project-structure)
6. [The Module System](#6-the-module-system)
7. [The Event Model](#7-the-event-model)
8. [The Policy Engine](#8-the-policy-engine)
9. [The Scanner](#9-the-scanner)
10. [The Enforcement Layer](#10-the-enforcement-layer)
11. [The Storage Layer](#11-the-storage-layer)
12. [The Transport Layer](#12-the-transport-layer)
13. [The IPC Layer](#13-the-ipc-layer)
14. [The User Helper (Tray / UI)](#14-the-user-helper-tray--ui)
15. [Platform Abstraction Layer](#15-platform-abstraction-layer)
16. [Configuration](#16-configuration)
17. [Logging & Telemetry](#17-logging--telemetry)
18. [Testing Strategy](#18-testing-strategy)
19. [Build System](#19-build-system)
20. [Packaging & Distribution](#20-packaging--distribution)
21. [Extensibility Roadmap](#21-extensibility-roadmap)
22. [Key Design Decisions](#22-key-design-decisions)
23. [Getting Started (Developer Setup)](#23-getting-started-developer-setup)
24. [Roadmap](#24-roadmap)
25. [Contributing](#25-contributing)
26. [License](#26-license)

---

## 1. Overview

**AxiomDLP Endpoint Agent** is the endpoint-side component of the AxiomDLP platform. It runs as a privileged background service on Linux and Windows endpoints, monitors user activity, detects sensitive data in real time, and enforces organization-defined DLP policies locally — even when offline.

It complements the existing AxiomDLP Core (Go/Gin API + PostgreSQL + PII/OCR/Extractor microservices) and the Postfix email milter. Where the milter handles email egress, the agent handles **endpoint egress**: file operations, USB writes, clipboard, printing, browser uploads, and more.

### Key Characteristics

- **Cross-platform** — first-class support for Linux and Windows from a single codebase.
- **Modular** — every capability is a plugin; new ones are added without touching the core.
- **Offline-first** — local policy cache and event queue. Works without the core.
- **Kernel-ready** — enforcement layer abstracts user-mode and kernel-mode backends.
- **Event-driven** — modules emit events; a central policy engine decides; enforcers act.
- **Secure** — mTLS/JWT authentication, OS-native secret storage, tamper resistance.
- **Observable** — structured logging, health checks, local metrics.

### Status

| Component | Status |
|-----------|--------|
| Project skeleton | Planned |
| Core framework | Planned |
| First module (filesystem) | Planned |
| Local policy engine | Planned |
| Transport layer | Stub (interface only) |
| Kernel enforcement | Future |

---

## 2. Design Philosophy

The agent is built around seven core beliefs:

1. **Separation of concerns** — capture, scan, decide, enforce, and report are five distinct stages. No stage knows more than it needs to.
2. **Platform abstraction** — business logic never calls OS APIs directly. All OS-specific code lives behind interfaces.
3. **Offline-first** — the agent must fully function without the core. Local policies, local scanning, local queueing.
4. **Kernel-ready** — the enforcement layer is abstracted so user-mode enforcement can be replaced with kernel-mode (Windows Minifilter / Linux eBPF LSM) without touching business logic.
5. **Fail-safe** — a single broken module must never crash the agent. Modules are isolated and supervised.
6. **Extensible by design** — new modules are dropped in and registered. No core modifications required.
7. **No global state** — everything flows through a shared `ServiceContext`. This makes testing and dependency injection trivial.

---

## 3. The Five Pipeline Stages

Every DLP event flows through the same pipeline:

```
[Capture] → [Scan] → [Decide] → [Enforce] → [Report]
```

| Stage | Responsibility | Implemented By |
|-------|---------------|----------------|
| **Capture** | Detect that something happened (file read, USB insert, clipboard copy, etc.) | Event source modules |
| **Scan** | Extract content and search for sensitive data | Scanner subsystem |
| **Decide** | Match scan result against local policies → decide action | Policy engine |
| **Enforce** | Block / alert / log the operation | Enforcer backends |
| **Report** | Queue event, upload to core | Storage + Transport |

Each stage is a separate subsystem. Modules only implement **Capture** (and optionally provide **Enforce** hooks). The rest is shared infrastructure.

---

## 4. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      AGENT SERVICE                          │
│                                                             │
│  ┌─────────────┐   ┌──────────────┐   ┌─────────────────┐  │
│  │  Module     │   │  Event Bus   │   │  Policy Engine  │  │
│  │  Manager    │──▶│              │──▶│                 │  │
│  └──────┬──────┘   └──────┬───────┘   └────────┬────────┘  │
│         │                 │                    │           │
│         ▼                 ▼                    ▼           │
│  ┌─────────────┐   ┌──────────────┐   ┌─────────────────┐  │
│  │  Modules    │   │  Scanner     │   │  Enforcer       │  │
│  │  (plugins)  │   │  (local/PII) │   │  (user/kernel)  │  │
│  └─────────────┘   └──────────────┘   └─────────────────┘  │
│                                                             │
│  ┌─────────────┐   ┌──────────────┐   ┌─────────────────┐  │
│  │  Storage    │   │  Transport   │   │  IPC Server     │  │
│  │  (SQLite)   │   │  (stub now)  │   │                 │  │
│  └─────────────┘   └──────────────┘   └────────┬────────┘  │
└──────────────────────────────────────────────────┼──────────┘
                                                   │
                                                   ▼
                                       ┌─────────────────────┐
                                       │  User Helper (tray) │
                                       └─────────────────────┘
```

The agent runs as a **privileged service**. A separate **user helper** process runs in each user session to provide tray UI and notifications, communicating with the service over IPC.

---

## 5. Project Structure

```
axiomdlp-agent/
├── CMakeLists.txt              # Root CMake
├── CMakePresets.json           # Build presets (debug/release, win/linux)
├── README.md
├── LICENSE
├── .clang-format
├── .clang-tidy
├── .gitignore
├── .gitmodules
│
├── cmake/                      # CMake helper modules
│   ├── FindLibcurl.cmake
│   ├── FindSQLite3.cmake
│   ├── FindOpenSSL.cmake
│   ├── FindDPAPI.cmake         # Windows
│   ├── FindLibsecret.cmake     # Linux
│   ├── CompilerWarnings.cmake
│   └── Packaging.cmake
│
├── external/                   # Third-party (submodules or FetchContent)
│   ├── nlohmann_json/
│   ├── spdlog/
│   ├── fmt/
│   ├── sqlite3/
│   ├── libcurl/
│   ├── openssl/
│   ├── boost/                  # Asio + Beast (for future WSS)
│   └── catch2/                 # Testing
│
├── src/
│   ├── main.cpp                # Entry point (service dispatcher)
│   │
│   ├── core/                   # Framework — no OS-specific code
│   │   ├── application
│   │   ├── module_manager
│   │   ├── event_bus
│   │   ├── service_context
│   │   ├── config
│   │   ├── logger
│   │   ├── version
│   │   └── error
│   │
│   ├── interfaces/             # Pure abstract interfaces
│   │   ├── i_module
│   │   ├── i_event_source
│   │   ├── i_enforcer
│   │   ├── i_policy_provider
│   │   ├── i_transport
│   │   ├── i_storage
│   │   ├── i_ipc
│   │   └── i_crypto
│   │
│   ├── event/                  # Event data model
│   │   ├── event
│   │   ├── event_types
│   │   ├── event_factory
│   │   └── event_serializer
│   │
│   ├── policy/                 # Policy engine (platform-agnostic)
│   │   ├── policy
│   │   ├── policy_engine
│   │   ├── policy_cache
│   │   ├── policy_sync
│   │   └── decision
│   │
│   ├── scanner/                # Local content scanning
│   │   ├── scanner
│   │   ├── regex_scanner
│   │   ├── local_pii_scanner
│   │   └── remote_pii_scanner
│   │
│   ├── enforcement/            # Enforcement orchestration
│   │   ├── enforcer
│   │   ├── enforcer_registry
│   │   └── backends/
│   │       ├── usermode/       # Current: user-mode blocking
│   │       └── kernel/         # Future: minifilter / eBPF
│   │
│   ├── storage/                # Local persistence
│   │   ├── database
│   │   ├── migrations
│   │   ├── event_queue
│   │   └── secure_store
│   │
│   ├── transport/              # Connection layer (stub for now)
│   │   ├── transport_manager
│   │   ├── http_client
│   │   ├── ws_client
│   │   ├── heartbeat
│   │   ├── reconnect
│   │   └── protocol
│   │
│   ├── ipc/                    # Service ↔ helper
│   │   ├── ipc_server
│   │   ├── ipc_client
│   │   └── messages
│   │
│   ├── telemetry/              # Internal metrics
│   │   ├── metrics
│   │   └── health
│   │
│   ├── platform/               # Platform Abstraction Layer
│   │   ├── platform.h
│   │   ├── windows/
│   │   │   ├── service         # SCM lifecycle
│   │   │   ├── named_pipe      # IPC
│   │   │   ├── dpapi           # Secure storage
│   │   │   ├── registry        # Config in registry
│   │   │   ├── process         # Process attribution
│   │   │   ├── file_watcher
│   │   │   ├── clipboard
│   │   │   ├── usb
│   │   │   └── printer
│   │   └── linux/
│   │       ├── systemd
│   │       ├── unix_socket
│   │       ├── keyring
│   │       ├── process
│   │       ├── file_watcher    # fanotify/inotify
│   │       ├── clipboard       # X11/Wayland
│   │       ├── usb             # udev
│   │       └── printer         # CUPS
│   │
│   └── modules/                # Feature modules (plugin-style)
│       ├── module_base
│       ├── filesystem/
│       ├── usb/
│       ├── clipboard/
│       ├── printer/
│       ├── browser/            # Placeholder for extension bridge
│       └── process/
│
├── kernel/                     # Future kernel components (empty now)
│   ├── windows/
│   │   └── minifilter/
│   └── linux/
│       └── ebpf/
│
├── helper/                     # User-session helper (tray UI)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main
│   │   ├── tray
│   │   ├── notifications
│   │   └── ipc_client
│   └── resources/
│
├── config/
│   ├── agent.default.json      # Default config
│   ├── agent.schema.json       # JSON schema for validation
│   └── logging.json
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── mocks/
│   └── CMakeLists.txt
│
├── scripts/
│   ├── build.sh
│   ├── build.ps1
│   ├── package_deb.sh
│   ├── package_rpm.sh
│   ├── package_msi.ps1
│   └── dev_mock_core.py        # Simple mock core for testing
│
├── packaging/
│   ├── linux/
│   │   ├── systemd/axiomdlp-agent.service
│   │   ├── debian/control
│   │   └── rpm/axiomdlp-agent.spec
│   └── windows/
│       ├── installer.wxs
│       └── service_install.ps1
│
└── docs/
    ├── architecture.md
    ├── modules.md
    ├── policy_format.md
    ├── event_schema.md
    └── ipc_protocol.md
```

---

## 6. The Module System

The module system is the heart of the agent's extensibility. Every capability — filesystem, USB, clipboard, print, browser, process — is a self-contained module.

### Module Contract

Every module implements the same base contract:

- `name()` — identifier (e.g., `"filesystem"`)
- `version()` — semantic version
- `initialize(context)` — receives shared services (config, logger, event bus, storage)
- `start()` — begins work
- `stop()` — pauses work
- `shutdown()` — releases resources
- `health()` — reports current health status

### Optional Roles

A module can additionally play one or more roles:

| Role | Meaning | Example Modules |
|------|---------|-----------------|
| **Event Source** | Emits events into the bus | filesystem, USB, clipboard |
| **Enforcer** | Can block/allow an operation | filesystem, clipboard |
| **Scanner** | Provides content scanning | regex, ML |
| **Transport** | Handles communication | HTTP, WebSocket |
| **Storage** | Persists data | SQLite |

A single module can play multiple roles. The filesystem module, for example, is **both** an Event Source and an Enforcer.

### Module Lifecycle

1. **Discovery** — the module manager scans the registry (or plugin directory later)
2. **Instantiation** — module object created
3. **Initialization** — receives context, config, dependencies
4. **Start** — begins emitting events
5. **Running** — reacts to config changes, health checks
6. **Stop** — stops emitting
7. **Shutdown** — releases resources

### Isolation & Supervision

- Each module runs in its own thread (or thread pool).
- The module manager supervises modules. If a module crashes, the manager logs and optionally restarts it.
- A broken module must never bring down the agent.

### Adding New Modules Later

To add USB support, you write a USB module that:

1. Implements the base module contract
2. Implements the Event Source role
3. Optionally implements the Enforcer role (block USB writes)

Then register it. No core changes required. Same for print, browser, process, network, etc.

---

## 7. The Event Model

An **Event** is a self-describing record of something that happened on the endpoint.

### Fields

- **Identity** — UUID, timestamp
- **Type** — `file_access`, `usb_insert`, `clipboard_copy`, `print_job`, `browser_upload`, `process_spawn`, etc.
- **Attribution** — user, process name, PID, session ID
- **Subject** — what was accessed (file path, device ID, clipboard content)
- **Payload** — optional content text or binary blob (may be redacted)
- **Metadata** — arbitrary key-value pairs for module-specific data

### Properties

- **Immutable** after creation
- **Serializable** (JSON now, Protocol Buffers later)
- **Auditable** — every event can be logged and uploaded
- **Versioned** — the schema is versioned so old agents keep working

Event types are declared in one place so all modules and the policy engine agree on vocabulary.

---

## 8. The Policy Engine

The policy engine is the brain of the agent. It receives events, evaluates them against local policies, and produces decisions.

### Policy Model

A policy contains:

- **Identifier** and human-readable **name**
- **Entity type** — `EMAIL`, `CREDIT_CARD`, `SSN`, `PHONE_NUMBER`, `PERSON`, `API_KEY`
- **Threshold** — minimum confidence to trigger
- **Action** — `block`, `alert`, `log`, `allow`
- **Priority** — higher wins in conflicts
- **Enabled flag**
- **Scope** — which modules / event types it applies to

### Evaluation Flow

1. Event arrives from the bus
2. Scanner produces a scan result (entities, risk score)
3. Policy engine loads applicable policies (scoped to module, event type, user)
4. Policies are evaluated **by priority**
5. The first matching policy determines the action
6. If no policy matches, the default action is `allow`

### Decision Model

A **Decision** contains:

- **Action** (block/alert/log/allow)
- **Risk score**
- **Reason** (human-readable)
- **Matching policy ID**
- **Enforce-synchronously flag** — whether the operation must be blocked before it completes

### Policy Sources

- **Local cache** — SQLite, always available
- **Bootstrap policies** — shipped with the installer, used before first sync
- **Remote policies** — fetched from the core (stub for now)
- **Versioning** — each policy set has a version; sync is by version

---

## 9. The Scanner

The scanner turns raw content into detections.

### Scanner Layers

| Layer | Description | When Used |
|-------|-------------|-----------|
| **Local regex scanner** | Fast, offline, deterministic. Detects credit cards, SSNs, emails, phone numbers using known patterns. | Always available |
| **Local ML scanner** (optional, later) | Embedded model for names, addresses, etc. | When enabled |
| **Remote PII scanner** | Calls the core's `/pii/detect` endpoint for deep analysis. | When online and needed |
| **Remote OCR/extractor** | For documents and images. | When online and needed |

### Strategy

- Prefer local scanning first (offline, fast).
- Fall back to remote when online and when local scanning is inconclusive.
- Cache remote results locally for audit.
- All scanners return a **unified scan result** so the policy engine doesn't care which scanner produced it.

### Unified Scan Result

- **Sensitive flag** — true/false
- **Risk score** — 0–10
- **Entity list** — each with type, position, confidence, matched text
- **Source** — which scanner produced it (`regex`, `ml`, `remote`)

---

## 10. The Enforcement Layer

Enforcement is abstracted behind an **Enforcer** interface. Backends are pluggable.

### Backend Tiers

| Tier | Windows | Linux | Capability |
|------|---------|-------|-----------|
| **User-mode** | API hooks, interposition | LD_PRELOAD, hooks | Block at app layer |
| **Kernel-mode** | Minifilter driver | eBPF LSM / fanotify | Block at syscall layer |
| **Firmware / Hypervisor** | Future | Future | Ultimate enforcement |

### Behavior

- The enforcer receives a **Decision** and the **Event** that triggered it.
- If `action = block`, it denies the operation **synchronously** (before it completes).
- If `action = alert`, it allows but raises a notification.
- If `action = log`, it allows silently and records the event.

### Why Abstract Now

Even though we start with user-mode only, the interface is designed so that swapping in a kernel driver later requires **zero changes** to the policy engine or event pipeline.

---

## 11. The Storage Layer

Local persistence uses **SQLite** with versioned migrations.

### What Is Stored

| Table | Purpose |
|-------|---------|
| **Event queue** | Events not yet uploaded to core |
| **Policy cache** | Latest synced policies |
| **Agent config** | Runtime configuration |
| **State** | Last heartbeat, policy version, connection state |
| **Audit log** | Local record of enforcement actions |
| **Agent identity** | Agent ID, organization ID (public) |

### Secure Storage (Separate)

Secrets are stored using OS-native mechanisms:

- **Windows** — DPAPI
- **Linux** — libsecret / kernel keyring

Secrets include: agent token, client certificate, private key, enrollment state.

---

## 12. The Transport Layer

Designed but not implemented in phase 1.

### Future Components

- **HTTP client** — for enrollment, heartbeat, policy fetch, event upload
- **WebSocket client** — for push channel (policy updates, commands)
- **Heartbeat loop** — periodic liveness
- **Reconnect logic** — exponential backoff with jitter
- **Protocol layer** — message schemas (JSON now, Protobuf later)
- **Auth manager** — JWT refresh, mTLS cert rotation

### Future Connection Model

- **Outbound only** — the agent dials out; the core never dials in. Works through NAT and firewalls.
- **WSS for control** — persistent WebSocket for push (policy updates, commands).
- **HTTPS for data** — stateless REST for event upload and policy fetch.
- **Heartbeat** — every 30–60 seconds, includes agent ID, version, policy version, queue size.
- **Reconnect** — exponential backoff (1s → 2s → 4s → … → 5 min) with jitter.
- **Fallback** — HTTPS long polling if WebSocket is blocked.
- **Offline queue** — events buffered locally and flushed on reconnect.

### Current Behavior (Phase 1)

For now, the transport layer exists as an **interface** with a **mock implementation** that:

- Logs everything it would send
- Reads responses from a local JSON file (for testing)
- Enables offline development

---

## 13. The IPC Layer

The agent runs as a **privileged service**. The **user helper** (tray icon, notifications) runs in the user session.

### Transport

- **Windows** — Named Pipes
- **Linux** — Unix Domain Sockets

### Messages

| Direction | Message | Purpose |
|-----------|---------|---------|
| Service → Helper | `show_notification` | Block/alert event notification |
| Service → Helper | `agent_status` | Health, version, connection state |
| Helper → Service | `user_dismiss` | User acknowledged notification |
| Helper → Service | `pause_agent` | User paused DLP (if allowed) |
| Helper → Service | `query_status` | Request current status |

### Security

- IPC channel is ACL-restricted (only service and helper can connect)
- Messages are length-prefixed and versioned
- No sensitive data crosses IPC in plaintext
- The helper cannot issue enforcement commands; it only observes and reports user intent

---

## 14. The User Helper (Tray / UI)

A separate process, running in each user session.

### Responsibilities

- Show tray icon (agent status: active, paused, error)
- Display notifications when an event is blocked
- Allow user to pause DLP (subject to policy)
- Show local agent health and version
- Relay user actions to service over IPC

### Why Separate

- The service runs as SYSTEM/root (no GUI).
- The helper runs as the logged-in user (has GUI).
- Clean separation of privilege and interface.
- If the helper crashes, the service keeps running and enforcing.

---

## 15. Platform Abstraction Layer

Every OS-specific capability lives behind an interface. Two implementations: `windows/` and `linux/`.

### Abstractions Provided

| Interface | Windows | Linux |
|-----------|---------|-------|
| Service lifecycle | SCM | systemd |
| IPC | Named Pipes | Unix Sockets |
| Secure storage | DPAPI | libsecret / keyring |
| File watching | `ReadDirectoryChangesW` / Minifilter | `fanotify` / `inotify` |
| Clipboard | Clipboard API + hooks | X11 / Wayland |
| USB events | `WM_DEVICECHANGE` / SetupAPI | `udev` / libudev |
| Printer | Spooler API | CUPS |
| Process info | Toolhelp / WMI | `/proc`, eBPF |

Business logic in `core/` never includes an OS header. This is enforced by code review and CI checks.

---

## 16. Configuration

### Sources (in order of precedence)

1. **Command-line arguments** (highest priority)
2. **Environment variables**
3. **Local config file** (JSON)
4. **Default config** (shipped with the installer)
5. **Core-pushed config** (future, lowest priority)

### Structure

- **Agent section** — ID, org, log level, data directory, heartbeat interval
- **Module sections** — per-module enable/disable and tuning
- **Policy section** — local policy overrides
- **Transport section** — core URL, auth, reconnect settings
- **Telemetry section** — metrics, health check intervals

### Validation

A JSON schema validates config on load. If invalid:

- The agent logs the error and falls back to defaults, **or**
- The agent refuses to start (configurable)

### Hot Reload

Where possible, config changes are applied at runtime without restarting the agent. Modules that cannot hot-reload are restarted individually.

---

## 17. Logging & Telemetry

### Logging

- **Structured JSON logs** via spdlog
- **Levels**: trace, debug, info, warn, error, critical
- **Local rolling log files** with rotation
- **Optional forwarding to core** (later)
- **Redaction** — sensitive content is never logged in plaintext

### Metrics

- Events captured (per module)
- Events blocked / alerted / logged
- Average scan latency
- Queue size
- Connection state
- Module health
- Memory and CPU usage

### Health

Each module reports health. The agent exposes an aggregate health status:

- Locally via IPC (for the helper)
- To the core via heartbeat (later)

---

## 18. Testing Strategy

### Unit Tests

- Each module independently
- Policy engine with mock events
- Scanner with sample content
- Storage migrations

### Integration Tests

- Full pipeline: event → scan → decide → enforce → store
- Mock transport, real everything else

### Mocks

- **Mock transport** — no network
- **Mock event source** — replays test events
- **Mock enforcer** — records decisions without blocking

### Test Data

- Sample sensitive strings (fake credit cards, SSNs, emails)
- Sample files (PDF, DOCX, images)
- Sample policies

### CI

- Runs on both Windows and Linux
- Unit + integration tests
- Static analysis (clang-tidy, cppcheck)
- Formatting checks (clang-format)

---

## 19. Build System

### Toolchain

- **CMake** ≥ 3.24
- **Ninja** (recommended) or Make
- **MSVC 2022** (Windows), **GCC 13** or **Clang 17** (Linux)

### Presets

- `debug` — Debug build with assertions
- `release` — Optimized production build
- `relwithdebinfo` — Optimized with debug symbols

### Third-Party Dependencies

Managed via **CMake FetchContent** or **git submodules**. Pinned to specific versions.

| Purpose | Library |
|---------|---------|
| JSON | nlohmann/json |
| Logging | spdlog + fmt |
| SQLite | sqlite3 |
| HTTP | libcurl |
| TLS/Crypto | OpenSSL |
| WebSocket (later) | Boost.Beast |
| Async I/O (later) | Boost.Asio |
| Testing | Catch2 |
| UUID | stduuid or Boost.UUID |

### Compiler Flags

- Warnings-as-errors in CI
- Sanitizers (ASan, UBSan) in debug builds
- Hardened runtime in release builds

---

## 20. Packaging & Distribution

### Linux

- **`.deb`** package (Debian / Ubuntu)
- **`.rpm`** package (RHEL / Fedora)
- **systemd unit file** for service lifecycle
- **Post-install script** — enrollment, service registration

### Windows

- **MSI installer** via WiX Toolset
- **Service registration** via PowerShell or MSI custom action
- **Silent install** for enterprise deployment (`msiexec /i ... /qn`)

### Code Signing

- Required on both platforms for production
- Windows — Authenticode
- Linux — package signing (GPG)

### Auto-Update

Planned for later. Design considerations:

- Signed update manifests
- Atomic update (write new binary, swap on restart)
- Rollback on failure
- Update server (part of core or separate)

---

## 21. Extensibility Roadmap

The structure is designed so these can be added without refactoring:

| Feature | How It Slots In |
|---------|-----------------|
| **USB module** | New event source + enforcer under `modules/usb/` |
| **Filesystem deep hook** | Swap user-mode backend for kernel minifilter |
| **Print module** | New event source under `modules/printer/` |
| **Browser module** | Native messaging bridge under `modules/browser/` |
| **Process module** | New event source under `modules/process/` |
| **Network module** | Outbound connection inspection under `modules/network/` |
| **Kernel drivers** | New code under `kernel/windows/` and `kernel/linux/` |
| **Local ML scanner** | New scanner under `scanner/` |
| **Remote policy sync** | Full transport layer under `transport/` |
| **Admin commands** | New message types on the push channel |
| **Remote wipe / kill switch** | Command handled by a dedicated module |

Each of these is a new folder and a new registration. Nothing else changes.

---

## 22. Key Design Decisions

These are the non-obvious architectural choices and why we made them.

1. **Event bus over direct calls** — decouples modules from the policy engine. A module doesn't know or care who processes its events.
2. **Async by default** — capture is non-blocking. Enforcement is the only synchronous step, and only when `action = block`.
3. **Local-first scanning** — offline capability is not optional. Remote calls are enhancements, not dependencies.
4. **Enforcement abstraction** — pays off enormously when kernel drivers are added. Business logic never changes.
5. **Separate helper process** — avoids running GUI as SYSTEM/root, which is a security smell.
6. **SQLite over custom formats** — battle-tested, transactional, queryable, portable.
7. **JSON now, Protobuf later** — JSON is easy to debug; Protobuf is for scale. The protocol layer abstracts this.
8. **Version everything** — config, policies, events, IPC messages. Rolling updates are possible without breaking old agents.
9. **Module registry is explicit** — no magic. Modules are listed in one place. Easier to audit, easier to reason about.
10. **No global state** — everything flows through `ServiceContext`. Makes testing trivial.
11. **Fail-open vs fail-closed** — configurable per policy. Timeouts default to fail-open (allow) so the agent never bricks a machine.
12. **No silent failures** — every module reports errors to the telemetry subsystem. The agent's health is always queryable.

---

## 23. Getting Started (Developer Setup)

### Prerequisites

- **Linux**: GCC 13+ or Clang 17+, CMake 3.24+, Ninja, `libcurl-dev`, `libssl-dev`, `libsqlite3-dev`, `libsecret-1-dev`
- **Windows**: Visual Studio 2022 (with C++ workload), CMake 3.24+, vcpkg (recommended), OpenSSL, libcurl, SQLite3

### Clone & Build

```bash
git clone --recursive https://github.com/your-org/axiomdlp-agent.git
cd axiomdlp-agent
cmake --preset=debug
cmake --build --preset=debug
```

### Run Tests

```bash
ctest --preset=debug
```

### Run the Agent (Development)

```bash
./build/debug/axiomdlp-agent --config ./config/agent.default.json --foreground
```

The `--foreground` flag runs the agent in the current terminal instead of installing it as a service. Useful for development.

### Run the Mock Core (for transport testing)

```bash
python3 scripts/dev_mock_core.py
```

This starts a local HTTP server that mimics the core's agent endpoints.

---

## 24. Roadmap

### Phase 1 — Foundation
- Project skeleton, CMake, CI
- Core framework (module manager, event bus, service context)
- Service lifecycle on both platforms
- Config loading and logging

### Phase 2 — First Module
- Filesystem module (Linux: inotify; Windows: ReadDirectoryChangesW)
- Local regex scanner
- Basic policy engine
- Local SQLite storage

### Phase 3 — Enforcement
- User-mode enforcement (block file operations)
- IPC with helper
- Tray UI and notifications

### Phase 4 — Connection (Stub → Real)
- Transport interface + mock
- HTTP client
- Enrollment, heartbeat, policy sync, event upload
- Reconnect logic

### Phase 5 — More Modules
- USB
- Clipboard
- Print
- Browser (via native messaging)

### Phase 6 — Kernel Enforcement
- Windows Minifilter driver
- Linux eBPF LSM / fanotify permission events

### Phase 7 — Production Hardening
- Auto-update
- Tamper resistance
- Code signing
- Performance tuning
- Enterprise deployment tooling

---

## 25. Contributing

### Code Style

- **C++20** — modern but not bleeding-edge
- **Formatting** — `clang-format` (see `.clang-format`)
- **Linting** — `clang-tidy` (see `.clang-tidy`)
- **Naming** — `snake_case` for functions and variables, `PascalCase` for types, `kConstantName` for constants
- **Headers** — include guards or `#pragma once` (project standard: `#pragma once`)
- **No raw `new`/`delete`** — use smart pointers
- **No exceptions across module boundaries** — modules return `Result<T>` types

### Pull Requests

- One feature per PR
- Tests required for new code
- CI must pass (build, tests, lint, format)
- Update docs if behavior changes

### Commit Messages

Follow Conventional Commits:

- `feat: add USB module skeleton`
- `fix: correct policy priority ordering`
- `docs: update architecture diagram`
- `refactor: extract scanner interface`

---

## 26. License

Apache License 2.0 — see the `LICENSE` file for details.

---

**AxiomDLP Endpoint Agent** — modular, cross-platform, offline-first DLP for endpoints.

For questions, open an issue or contact the maintainers.