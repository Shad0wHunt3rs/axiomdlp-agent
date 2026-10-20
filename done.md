# AxiomDLP Endpoint Agent

Cross-platform endpoint data-loss-prevention agent written in C++20. Watches
the filesystem, inventories files, hashes content, stores metadata in SQLite,
connects to the AxiomDLP Core server over TLS (gRPC for server-pushed
messages, HTTPS for enrollment/heartbeat/scan), and shuts down atomically
with a typed reason.

**Current status:** Phase 5 of the gRPC migration — the transport layer
has been migrated to gRPC and WebSocket has been removed. `agent_sync`
and `content_scanner` remain on HTTP pending Phase 5.4–5.5 of the
migration roadmap.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Current Status](#2-current-status)
3. [Key Features](#3-key-features)
4. [Architecture](#4-architecture)
5. [Supported Platforms](#5-supported-platforms)
6. [Dependencies](#6-dependencies)
7. [Prerequisites](#7-prerequisites)
8. [Building on Linux](#8-building-on-linux)
9. [Building on Windows](#9-building-on-windows)
10. [Configuration](#10-configuration)
11. [Running the Agent](#11-running-the-agent)
12. [Project Structure](#12-project-structure)
13. [Modules Reference](#13-modules-reference)
14. [Data Flow](#14-data-flow)
15. [TLS and Security](#15-tls-and-security)
16. [Shutdown Model](#16-shutdown-model)
17. [Testing](#17-testing)
18. [Roadmap](#18-roadmap)
19. [Known Issues and Limitations](#19-known-issues-and-limitations)
20. [Contributing](#20-contributing)
21. [Related Documents](#21-related-documents)

---

## 1. Overview

AxiomDLP Endpoint Agent is the client-side component of the AxiomDLP
data-loss-prevention platform. It runs as a system service on Linux
(systemd) and Windows (Service Control Manager) and is responsible for:

- Observing file-system activity in real time
- Building and maintaining a local inventory of files with content hashes
- Sending file content chunks to the Core server for PII detection
- Receiving server-pushed policy updates, commands, and revocation notices
- Reporting agent identity and liveness via periodic heartbeats
- Shutting down cleanly when instructed by the server, the user, or an
  internal failure

The agent is designed as an extensible foundation. The current release
covers the "observe and report" layer. Enforcement (blocking, quarantining,
redacting), rich-document extraction, kernel-level hooks, and a user-facing
tray helper are planned for the next product phase (see `next.md`).

The codebase is organized around a strict separation between:

- **Framework** — `foundation/`, `events/`, `modules/`
- **Feature modules** — `features/<name>/`
- **Platform bindings** — `platform/<os>/`
- **Wire protocol** — `features/grpc_transport/agent.proto`

Every capability the agent offers is implemented as a module. Adding a new
capability means adding a folder under `src/features/` and registering it in
`application.cpp`. The framework itself does not change.

---

## 2. Current Status

### 2.1 What is complete

- Cross-platform service life cycle (systemd on Linux, SCM on Windows,
  foreground mode for both)
- Four verified build targets from two independent toolchains
- Real-time filesystem watching via efsw (inotify on Linux,
  ReadDirectoryChangesW on Windows)
- Periodic full-filesystem inventory with SHA-256 hashing
- SQLite storage with automatic schema migrations (version 4)
- gRPC `Channel` bidirectional stream to the Core with TLS, jittered
  reconnect, and status-code-aware handling
- HTTP enrollment (`POST /api/v1/agent/register`), heartbeat
  (`POST /api/v1/agent/heartbeat`), and content scan
  (`POST /api/v1/agent/scan/batch`) using cpp-httplib with unconditional
  TLS verification
- Content scanner for text files: overlapping chunking, per-chunk verdicts,
  hash-based deduplication, local persistence of results
- Atomic shutdown with typed reasons preserved end to end
- Server-pushed revocation (`revoke` message from the gRPC stream triggers
  immediate shutdown)
- Fully static Windows binaries with static CRT (`/MT`) — no runtime
  redistributable required

### 2.2 What is pending (Phase 5 remainder)

The following items from the Phase 5 migration roadmap are still open:

- `agent_sync` still uses HTTP unary calls instead of gRPC unary RPCs
  (`Register`, `Heartbeat`) defined in `agent.proto`
- `content_scanner` still uses HTTP with base64-encoded chunk payloads
  instead of the gRPC `ScanBatch` method with native `bytes` payloads
- Server-pushed messages are dispatched inline in
  `grpc_transport_module.cpp` rather than through a unified `MessageRouter`
- No `server.transport` config selector — the agent runs the gRPC Channel
  and the HTTP sync path simultaneously, always
- `UNIMPLEMENTED` gRPC status is not wired to `FatalError`
- Windows binary sizes need re-measuring after gRPC and protobuf became
  mandatory build dependencies

### 2.3 What does not exist yet (Phase 2 product roadmap)

- Local policy engine and decision evaluation
- User-mode enforcement (file, clipboard, print)
- Kernel-mode enforcement (Linux eBPF, Windows Minifilter)
- Rich-document extraction (PDF, DOCX, XLSX, OCR)
- Advanced detection (EDM, PDM, document registry)
- IPC channel and user-session tray helper
- Remote command handling and auto-update
- Code signing and packaging (`.deb`, `.rpm`, MSI)
- macOS and ARM64 support
- Observability (Prometheus metrics) and hardening

---

## 3. Key Features

### 3.1 Filesystem awareness

- Real-time watcher with 300 ms trailing-edge debounce
- Event coalescing by priority: `modified > created > moved > deleted`
- Two-tier exclusion (glob patterns plus directory prefixes)
- Self-exclusion of the agent's own log and data directories
- Periodic full inventory for completeness (catches events the watcher
  missed, including events that occurred while the agent was stopped)

### 3.2 Content fingerprinting

- SHA-256 hashing streamed from disk in 1 MB chunks — constant memory
  regardless of file size
- Hash stored in the local `files` table and used for verdict deduplication
  (a copy or move of an already-scanned file is skipped)
- Content itself is never persisted; it is read into memory, transmitted,
  and discarded

### 3.3 Server connectivity

- **gRPC Channel bidi stream** — the primary server-push channel.
  Carries `policy_update`, `command`, and `revoke` messages. Sends a
  typed `ClientShutdown` on graceful stop.
- **HTTP enrollment and heartbeat** — cpp-httplib over HTTPS with
  unconditional certificate verification. Immediate validation heartbeat
  at startup with a saved `agent_id` so a revoked token is detected
  within milliseconds.
- **HTTP scan batch** — chunked file content sent to
  `POST /api/v1/agent/scan/batch`, verdicts merged back into the local
  `files` table.

### 3.4 Reliability

- Exponential backoff with 20 % jitter for reconnect — prevents
  thundering-herd behavior across a large fleet
- Interruptible sleep that wakes on shutdown request
- Per-module health status aggregated by `ModuleManager`
- Fail-safe configuration: bad config never prevents startup
- Async structured logging with non-blocking delivery and rotation

### 3.5 Deployment

- Four build targets from two toolchains
- Fully static on both platforms — no shared library dependencies at
  runtime beyond OS system DLLs on Windows
- Windows binaries use static CRT — no `vcruntime140.dll` required
- Same source tree, same `find_package(...)` API, same imported targets
  on both paths

---

## 4. Architecture

### 4.1 High-level diagram

```
┌─────────────────────────────────────────────────────────────┐
│ Application                                                 │
│  Owns: Config, Logger, ShutdownController, Database,        │
│        FileStore, StateStore, EventBus, ModuleContext,      │
│        ModuleManager, Service                               │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Service                                                     │
│  Linux:   systemd + sd_notify + signal handlers             │
│  Windows: SCM + ServiceMain + control handler               │
│  Blocks on ShutdownController::wait(); tears down on request│
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ ModuleManager                                               │
│  Lifecycle: initialize → start → stop → shutdown (reverse)  │
└──────────────────────────┬──────────────────────────────────┘
                           │
   ┌─────────┬─────────────┼──────────────┬───────────────┐
   ▼         ▼             ▼              ▼               ▼
┌──────┐ ┌─────────┐ ┌──────────┐ ┌────────────┐ ┌────────────┐
│ FS   │ │Inventory│ │Event     │ │gRPC        │ │AgentSync + │
│Watch │ │         │ │Logger    │ │Transport   │ │ContentScan │
└──┬───┘ └────┬────┘ └────┬─────┘ └─────┬──────┘ └─────┬──────┘
   │          │           │             │              │
   │ publish  │ hash      │ subscribe   │ Channel      │ HTTP + TLS
   ▼          ▼           ▼             ▼              ▼
┌──────┐ ┌─────────┐ ┌──────────┐ ┌────────────┐ ┌────────────┐
│Event │ │FileStore│ │EventBus  │ │Core gRPC   │ │Core HTTP   │
│Bus   │ │(SQLite) │ │          │ │(Channel)   │ │(register,  │
│      │ │         │ │          │ │            │ │ heartbeat, │
│      │ │         │ │          │ │            │ │ scan/batch)│
└──────┘ └─────────┘ └──────────┘ └────────────┘ └────────────┘
```

### 4.2 Layered source tree

- `foundation/` — application orchestration, config, logging, shutdown
- `events/` — event model and in-process publish/subscribe bus
- `modules/` — module framework (interface, base, context, manager, health)
- `features/` — one folder per capability
- `platform/` — OS-specific implementations behind interfaces
- `net/` — HTTP client (and reserved space for future transport helpers)
- `util/` — hashing, glob matching, base64
- `storage/` — SQLite wrapper, migrations, repositories

Higher layers depend on lower layers. Lower layers never depend on higher
layers. Business logic never includes OS-specific headers.

### 4.3 Design invariants

1. **Every capability is a module.** Adding a feature is adding a folder
   under `src/features/<name>/` and one registration line in
   `application.cpp`.
2. **Modules communicate through the event bus.** No direct calls between
   feature modules.
3. **One shutdown authority.** `ShutdownController` is the only place a
   shutdown is requested. Every module reads the same reason.
4. **Fail-safe.** Bad config, missing paths, and failed modules never
   crash the agent.
5. **RAII everywhere.** No manual resource management.
6. **Platform isolation.** Only files under `src/platform/<os>/` include
   OS-specific headers.

---

## 5. Supported Platforms

Four build targets, all verified to build, link, and run:

| Platform       | Toolchain             | Triplet / Toolchain File             |
| -------------- | --------------------- | ------------------------------------ |
| Linux x86_64   | GCC + `.deps/` cache  | `cmake/toolchains/linux-x86_64.cmake` |
| Linux i686     | GCC `-m32` + `.deps/` | `cmake/toolchains/linux-i686.cmake`   |
| Windows x86_64 | MSVC 2022 + vcpkg     | `x64-windows-static`                  |
| Windows x86    | MSVC 2022 + vcpkg     | `x86-windows-static`                  |

All four targets support TLS, the gRPC Channel transport, HTTP
enrollment/heartbeat/scan, filesystem watching, hashing, SQLite storage,
and graceful shutdown.

---

## 6. Dependencies

### 6.1 Per-component dependency table

| Component        | Linux                                     | Windows                            |
| ---------------- | ----------------------------------------- | ---------------------------------- |
| HTTP client      | cpp-httplib 0.15.3 (FetchContent)         | cpp-httplib 0.15.3 (FetchContent)  |
| gRPC             | `.deps/grpc-linux-*` (built locally)      | vcpkg `grpc`                       |
| Protobuf         | built with gRPC                           | vcpkg `protobuf`                   |
| TLS / crypto     | `.deps/openssl-linux-*` (built locally)   | vcpkg `openssl`                    |
| SQLite           | amalgamation from `.deps/sqlite3-src`     | vcpkg `sqlite3`                    |
| JSON             | system package or FetchContent            | vcpkg `nlohmann-json`              |
| Logging          | FetchContent spdlog 1.14.1                | vcpkg `spdlog` + `fmt`             |
| File watching    | FetchContent efsw 1.7.2                   | vcpkg `efsw`                       |
| Service manager  | libsystemd (via pkg-config, optional)     | SCM (part of `advapi32`)           |

### 6.2 Removed dependencies

The following were dependencies in earlier revisions and have been removed:

- **Boost 1.90 / 1.92** — no longer used anywhere
- **Boost.Asio** — replaced by gRPC for async I/O
- **Boost.Beast** — replaced by cpp-httplib for HTTP; WebSocket path
  deleted entirely

### 6.3 Why gRPC and Protobuf are mandatory

The Core server exposes a `Channel` bidirectional RPC that is the primary
server-to-agent push channel. It carries policy updates, remote commands,
and revocation notices. The stream is implemented and used; it is not
optional scaffolding.

---

## 7. Prerequisites

### 7.1 Linux

Tested on Debian, Ubuntu, and Kali Linux. Install with:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y \
    build-essential \
    gcc-multilib g++-multilib \
    ninja-build git file clangd \
    libsystemd-dev libsystemd-dev:i386 \
    pkg-config curl make
```

Notes:

- `gcc-multilib` and `g++-multilib` are required only for the i686 target
- `libsystemd-dev:i386` is required for the i686 target's `sd_notify`
  integration
- `curl` is used by `scripts/linux/build-openssl.sh` for the source
  download

### 7.2 Windows

Required:

- **Visual Studio 2022** with the "Desktop development with C++"
  workload (Build Tools also works)
- **CMake 3.24 or newer** on `PATH`
- **Ninja** on `PATH`
- **vcpkg** cloned to `C:\Users\<user>\vcpkg` with the `VCPKG_ROOT`
  environment variable set — the setup script handles this automatically
- **Long paths enabled** — set
  `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled`
  to `1`

Optional but recommended:

- PowerShell 7 (the setup scripts work with PowerShell 5.1 but assume
  ASCII-only file content)

---

## 8. Building on Linux

### 8.1 One-shot build

```bash
./scripts/linux/build-all.sh release
```

On first run this script:

1. Clones the nlohmann/json source into `.deps/nlohmann_json-src`
   if missing
2. Runs `scripts/linux/build-openssl.sh` if either OpenSSL tree is
   incomplete — downloads and builds OpenSSL 3.3.2 for both
   `linux-x86_64` and `linux-i686` (takes 15–25 minutes)
3. Runs `scripts/linux/build-grpc.sh` if either gRPC tree is incomplete —
   downloads gRPC 1.62.0 with submodules, builds host tools (`protoc`,
   `grpc_cpp_plugin`), then builds gRPC statically for both Linux targets
   (takes 30–60 minutes)
4. Configures and builds the agent for both targets

On subsequent runs, everything is incremental and completes in around
30 seconds.

### 8.2 Manual build

```bash
cmake --preset linux-x86_64-release
cmake --build --preset linux-x86_64-release

cmake --preset linux-i686-release
cmake --build --preset linux-i686-release
```

### 8.3 Available presets

| Preset                         | Build type | Output                                        |
| ------------------------------ | ---------- | --------------------------------------------- |
| `linux-x86_64-debug`           | Debug      | `build/linux-x86_64-debug/axiomdlp-agent`     |
| `linux-x86_64-release`         | Release    | `build/linux-x86_64-release/axiomdlp-agent`   |
| `linux-i686-debug`             | Debug      | `build/linux-i686-debug/axiomdlp-agent`       |
| `linux-i686-release`           | Release    | `build/linux-i686-release/axiomdlp-agent`     |

### 8.4 Verifying the build

```bash
file build/linux-x86_64-release/axiomdlp-agent
# ELF 64-bit LSB pie executable, x86-64

file build/linux-i686-release/axiomdlp-agent
# ELF 32-bit LSB pie executable, Intel 80386

ldd build/linux-x86_64-release/axiomdlp-agent | grep -E 'ssl|grpc|protobuf'
# (empty — all static)
```

---

## 9. Building on Windows

### 9.1 One-time setup

```powershell
.\scripts\win\setup-windows-msvc.ps1
```

This script:

1. Verifies that Visual Studio 2022 with the C++ workload is present
2. Downloads vcpkg as a ZIP from `codeload.github.com` (about 25 MB) —
   avoids a slow `git clone`
3. Downloads a prebuilt `vcpkg.exe` from `vcpkg-tool/releases` (about
   3 MB) — avoids a bootstrap compile
4. Sets `VCPKG_ROOT` persistently for the current user

Total runtime is approximately 3 minutes on a normal connection. The
script uses `Invoke-WebRequest` and falls back to `setx` if the .NET
environment API fails.

### 9.2 Build

```powershell
# x64 Release
.\scripts\win\build-windows-msvc.ps1 -Arch x64

# x86 Release
.\scripts\win\build-windows-msvc.ps1 -Arch x86

# Debug
.\scripts\win\build-windows-msvc.ps1 -Arch x64 -Configuration Debug
```

The build script:

- Automatically initializes the Visual Studio environment via
  `vcvarsall.bat`, so it works from any PowerShell window (a Developer
  PowerShell prompt is not required)
- Preserves `VCPKG_ROOT` across Visual Studio environment initialization
  (otherwise `vcvarsall.bat` would overwrite it with the VS-bundled
  vcpkg, which requires `builtin-baseline` and lacks the required ports)
- Sets `CC=cl.exe` and `CXX=cl.exe` to override any GCC found on `PATH`
  (for example MSYS2's toolchain)
- Detects and cleans stale `CMakeCache.txt` files that reference a
  non-MSVC compiler
- Cleans the build directory when `-Clean` is passed

First Windows build takes 40–60 minutes per architecture while vcpkg
compiles the full dependency tree (OpenSSL, gRPC, protobuf, abseil,
c-ares, re2, upb, spdlog, fmt, nlohmann-json, efsw, SQLite,
cpp-httplib). Subsequent builds restore from vcpkg's binary cache in
5–10 seconds.

### 9.3 Available presets

| Preset                            | Build type | Output                                           |
| --------------------------------- | ---------- | ------------------------------------------------ |
| `windows-x64-msvc-debug`          | Debug      | `build/windows-x64-msvc-debug/axiomdlp-agent.exe` |
| `windows-x64-msvc-release`        | Release    | `build/windows-x64-msvc-release/axiomdlp-agent.exe` |
| `windows-x86-msvc-debug`          | Debug      | `build/windows-x86-msvc-debug/axiomdlp-agent.exe` |
| `windows-x86-msvc-release`        | Release    | `build/windows-x86-msvc-release/axiomdlp-agent.exe` |

### 9.4 Verifying the build

```powershell
# Confirm architecture
dumpbin /headers build\windows-x64-msvc-release\axiomdlp-agent.exe |
    Select-String "machine"

# Confirm no external DLL dependencies
dumpbin /dependents build\windows-x64-msvc-release\axiomdlp-agent.exe
# Expected: KERNEL32, ADVAPI32, WS2_32, SHELL32, ole32, USER32, CRYPT32
```

### 9.5 Troubleshooting vcpkg downloads

If a vcpkg source download stalls (a known issue on slow links to the
GitHub CDN), the fallback is to download the file manually in WSL using
a parallel downloader, then copy it into the vcpkg downloads directory:

```bash
# In WSL
aria2c -x16 -s16 -d /tmp -o <filename> <url>
```

```powershell
# In PowerShell
copy \\wsl$\<distro>\tmp\<filename> $env:USERPROFILE\vcpkg\downloads\
```

vcpkg will pick up the pre-cached file silently. This workaround is
documented in `done.md` under Bugs W18–W20.

---

## 10. Configuration

### 10.1 Location

The agent looks for its configuration in the following order:

1. Path passed via `--config <path>` on the command line
2. `config/agent.default.json` relative to the current working directory
3. Platform default:
   - Linux: `/etc/axiomdlp/agent.json`
   - Windows: `%ProgramData%\AxiomDLP\agent.json`

If no configuration file is found, the agent starts with built-in
defaults and logs a warning. It never fails to start because of a
missing or invalid configuration file.

### 10.2 Structure

Configuration is a single JSON object with four top-level sections:

```json
{
  "logging": { ... },
  "storage": { ... },
  "server":  { ... },
  "modules": { ... }
}
```

### 10.3 The `server` section

All server-facing configuration lives in one place. Every module that
talks to the Core reads from this section.

```json
"server": {
  "base_url": "http://127.0.0.1:3000",
  "api_token": "axdlp_user_...",
  "heartbeat_interval_seconds": 60,
  "reconnect_backoff_seconds": 10,
  "reconnect_backoff_max_seconds": 300,
  "grpc_port": 50051,
  "grpc_tls": true,
  "grpc_ca_path": ""
}
```

| Field                          | Type    | Default                 | Purpose                                         |
| ------------------------------ | ------- | ----------------------- | ----------------------------------------------- |
| `base_url`                     | string  | `http://127.0.0.1:3000` | Core base URL; derives gRPC target host         |
| `api_token`                    | string  | `""`                    | Bearer token (`axdlp_user_...`)                 |
| `heartbeat_interval_seconds`   | integer | `60`                    | Heartbeat period for `agent_sync`               |
| `reconnect_backoff_seconds`    | integer | `10`                    | Initial backoff for reconnect                   |
| `reconnect_backoff_max_seconds`| integer | `300`                   | Maximum backoff cap                             |
| `grpc_port`                    | integer | `50051`                 | gRPC service port                               |
| `grpc_tls`                     | boolean | `true`                  | Enable TLS on the gRPC channel                  |
| `grpc_ca_path`                 | string  | `""`                    | Optional PEM CA bundle; empty means system roots |

`grpc_tls` should only be set to `false` in local development. Production
deployments must keep it at `true`.

### 10.4 Module sections

Each module reads its own subsection under `modules`:

- `modules.filesystem_watcher` — watch paths, exclude patterns, debounce
  window
- `modules.inventory` — scan interval, hash policy, size limits,
  scannable categories
- `modules.agent_sync` — `enabled` only; all other fields come from
  `server`
- `modules.content_scanner` — batch size, chunk size, overlap, retry
  limits, text-extension allowlist

See `config/agent.schema.json` for the full schema.

### 10.5 Environment variable expansion

String values in the configuration can reference environment variables:

- Windows: `%VAR%`
- POSIX: `${VAR}`

Example:

```json
"watch_paths": [ "%USERPROFILE%" ]
```

Expansion happens at load time. Unresolved variables are passed through
unchanged.

### 10.6 Fail-safe behavior

The following are handled without aborting startup:

- Missing configuration file
- Malformed JSON
- Wrong-typed fields
- Out-of-range integer values
- Invalid enum values

Every problem produces a warning in the log and a fall-back to the safe
default for that field. The agent always starts.

### 10.7 Deprecated fields

`verify_tls` at any level produces a warning that the field is ignored.
The agent always verifies TLS certificates and hostnames on `https://`
and `wss://`.

`modules.transport.*` and legacy fields under `modules.agent_sync.*`
are still read for backwards compatibility but the canonical location is
the top-level `server` section.

---

## 11. Running the Agent

### 11.1 Foreground mode (development)

Linux:

```bash
./build/linux-x86_64-release/axiomdlp-agent \
    --foreground \
    --config config/agent.default.json
```

Windows:

```powershell
.\build\windows-x64-msvc-release\axiomdlp-agent.exe `
    --foreground `
    --config config\agent.default_win.json
```

Console output is colored and always available. File output goes to
`logs/agent.log` with rotation.

### 11.2 Service mode (production)

Linux:

```bash
sudo ./build/linux-x86_64-release/axiomdlp-agent --install
```

This writes a systemd unit file to
`/etc/systemd/system/axiomdlp-agent.service`, runs `systemctl
daemon-reload`, and enables the service.

Windows:

```powershell
.\build\windows-x64-msvc-release\axiomdlp-agent.exe --install
```

This registers the service with the Service Control Manager, sets it to
auto-start, and starts it immediately.

### 11.3 Uninstalling

Linux:

```bash
sudo ./build/linux-x86_64-release/axiomdlp-agent --uninstall
```

Windows:

```powershell
.\build\windows-x64-msvc-release\axiomdlp-agent.exe --uninstall
```

### 11.4 Command line arguments

| Argument        | Meaning                                                |
| --------------- | ------------------------------------------------------ |
| `--install`     | Install as a system service and exit                   |
| `--uninstall`   | Remove the service and exit                            |
| `--foreground`  | Run in foreground mode (blocks; logs to console)       |
| `--config PATH` | Use this configuration file instead of the default     |

Any unrecognized argument produces a warning and is ignored.

### 11.5 Logs

Logs are written in the format:

```
[YYYY-MM-DD HH:MM:SS.mmm] [level] [t:thread-id] message
```

Levels follow standard severity: `trace`, `debug`, `info`, `warn`,
`error`, `critical`. The default level is `info`. Rotation is configured
for 5 MB per file and 3 historical files (approximately 20 MB total).

---

## 12. Project Structure

```
axiomdlp-agent/
├── CMakeLists.txt                       Top-level build definition
├── CMakePresets.json                    Preset definitions for all targets
├── vcpkg.json                           Windows dependency manifest
├── README.md                            This document
├── done.md                              Implementation log and bug history
├── next.md                              Phase 2 product roadmap
├── LICENSE
├── .clangd
├── .gitignore
│
├── .deps/                               Linux dependency cache (gitignored)
│   ├── spdlog-src/
│   ├── nlohmann_json-src/
│   ├── efsw-src/
│   ├── cpp-httplib-src/
│   ├── sqlite3-src/
│   ├── openssl-src/
│   ├── openssl-linux-x86_64/
│   ├── openssl-linux-i686/
│   ├── grpc-src/
│   ├── grpc-host-tools/
│   ├── grpc-linux-x86_64/
│   └── grpc-linux-i686/
│
├── vcpkg_installed/                     Windows dependency cache (gitignored)
│   ├── x64-windows-static/
│   └── x86-windows-static/
│
├── cmake/
│   └── toolchains/
│       ├── linux-x86_64.cmake
│       └── linux-i686.cmake
│
├── config/
│   ├── agent.default.json
│   ├── agent.default_win.json
│   └── agent.schema.json
│
├── data/                                Runtime SQLite database (gitignored)
├── logs/                                Runtime logs (gitignored)
│
├── scripts/
│   ├── linux/
│   │   ├── build-all.sh
│   │   ├── build-grpc.sh
│   │   ├── build-openssl.sh
│   │   └── merge-compile-commands.sh
│   └── win/
│       ├── setup-windows-msvc.ps1
│       └── build-windows-msvc.ps1
│
├── packaging/
│   ├── linux/systemd/axiomdlp-agent.service
│   └── windows/service_install.ps1
│
├── docs/
│   └── file_structure.md                (stale — see §19)
│
└── src/
    ├── main.cpp
    │
    ├── foundation/
    │   ├── application.{h,cpp}
    │   ├── config.{h,cpp}
    │   ├── logger.{h,cpp}
    │   └── shutdown.h
    │
    ├── events/
    │   ├── event.{h,cpp}
    │   └── event_bus.{h,cpp}
    │
    ├── modules/
    │   ├── i_module.h
    │   ├── module_base.h
    │   ├── module_context.h
    │   ├── module_manager.{h,cpp}
    │   └── health.h
    │
    ├── platform/
    │   ├── paths.h
    │   ├── service.h
    │   ├── file_watcher.h
    │   ├── common/
    │   │   └── efsw_file_watcher.cpp
    │   ├── linux/
    │   │   ├── paths.cpp
    │   │   └── service.cpp
    │   └── windows/
    │       ├── paths.cpp
    │       └── service.cpp
    │
    ├── net/
    │   ├── http_client.{h,cpp}
    │   └── outbound_queue.{h,cpp}
    │
    ├── util/
    │   ├── glob_match.{h,cpp}
    │   ├── file_hash.{h,cpp}
    │   └── base64.{h,cpp}
    │
    ├── storage/
    │   ├── sqlite_database.{h,cpp}
    │   ├── migrations.{h,cpp}
    │   ├── file_repository.{h,cpp}
    │   └── state_repository.{h,cpp}
    │
    └── features/
        ├── filesystem_watcher/
        │   └── filesystem_watcher_module.{h,cpp}
        ├── inventory/
        │   ├── inventory_module.{h,cpp}
        │   └── inventory_scanner.{h,cpp}
        ├── event_logger/
        │   └── event_logger_module.{h,cpp}
        ├── agent_sync/
        │   └── agent_sync_module.{h,cpp}
        ├── content_scanner/
        │   └── content_scanner_module.{h,cpp}
        └── grpc_transport/
            ├── agent.proto
            └── grpc_transport_module.{h,cpp}
```

---

## 13. Modules Reference

### 13.1 `filesystem_watcher`

**Purpose:** Observe real-time filesystem changes.

**Implementation:** `efsw` library (inotify on Linux,
ReadDirectoryChangesW on Windows).

**Behavior:**

- 300 ms trailing-edge debounce (configurable)
- Coalesces multiple events on the same path within the debounce window
- Priority merge: `modified` (4) > `created` (3) > `moved` (2) >
  `deleted` (1)
- Self-excludes the agent's own log and data directories to prevent a
  feedback loop
- Two-tier exclusion: glob patterns plus explicit directory prefixes
- Watcher initialization runs asynchronously so slow recursive watches
  don't block service startup

**Events published:** `file_write`, `file_delete`, `file_access`
(depending on the underlying event type)

### 13.2 `inventory`

**Purpose:** Periodic full-filesystem scan to keep the `files` table
complete.

**Behavior:**

- Runs on a configurable interval (default 900 s)
- Captures rich metadata: path decomposition, nanosecond timestamps,
  POSIX mode, uid/gid, nlink, file type, category
- Computes SHA-256 for files that pass the scanner's eligibility filter
- Assigns explicit `scan_status`:
  - `pending` — file is eligible for scanning, has a hash, waiting for
    the content scanner
  - `not_applicable` — file is not a regular text file (directory,
    symlink, binary format)
  - `skipped_too_large` — exceeds the configured size limit
  - `scan_error` — hashing failed
- Batches inserts in groups of 5,000 rows inside a single transaction
- Graceful stop within about 20 ms of a shutdown request; partial
  progress is committed

**Events published:** none directly (writes to the `files` table)

### 13.3 `event_logger`

**Purpose:** Observability.

**Behavior:** Subscribes to all events on the bus and logs them at INFO
level. No business logic. Provides a complete trace of what the agent is
processing.

**Events published:** none

### 13.4 `agent_sync`

**Purpose:** Agent identity management and liveness.

**Behavior:**

- Enrolls with the Core via `POST /api/v1/agent/register` on first run
- Stores `agent_id` in the local `state` table
- Sends a heartbeat every `server.heartbeat_interval_seconds` (default
  60 s)
- Sends an immediate validation heartbeat at startup when a saved
  `agent_id` exists — a revoked token is detected within milliseconds
- Interruptible sleep — wakes immediately on shutdown request
- HTTP client: cpp-httplib with unconditional certificate verification

**Error handling:**

| HTTP status | Action |
| --- | --- |
| 200 | Heartbeat recorded; continue |
| 401 | `ShutdownController::request(AuthRevoked)` — token invalid, expired, or revoked |
| 403 | `ShutdownController::request(AgentRevoked)` — this specific agent revoked |
| 404 | Clear `agent_id` from `state`, trigger re-enrollment |
| 5xx | Log and retry on next interval |
| Connection error | Log and retry on next interval |

**Planned migration:** Phase 5.4 will move to gRPC unary `Register` and
`Heartbeat` RPCs defined in `agent.proto`.

### 13.5 `content_scanner`

**Purpose:** Detect PII in text files by sending content chunks to the
Core and merging the verdicts.

**Behavior:**

- Queries `fetch_pending_for_scan` on the local `files` table
- Filters by extension (against `text_extensions`), size, existence
- Computes SHA-256 of the current content; if an identical hash was
  previously scanned, reuses the cached verdict without an HTTP call
- Splits content into overlapping chunks:
  - Default 1 MB chunk size, 256-byte overlap
  - Overlap prevents PII from straddling a chunk boundary
- Batches chunks to respect `batch_size` (max chunks per request) and
  `batch_max_bytes` (max estimated payload per request)
- Sends to `POST /api/v1/agent/scan/batch` with base64-encoded content
- Merges per-chunk verdicts by `file_hash + chunk_index`
- Writes results back to the `files` table via
  `update_scan_result_by_path`
- Content is never persisted on disk; read into memory, transmitted,
  and discarded

**Error handling:**

| Condition | Action |
| --- | --- |
| 401 / 403 | Log error, stop scanning, mark module as failed |
| 5xx | Leave file `pending`; retry next cycle |
| Chunk-level error | Leave file `pending`; do not abort the whole batch |
| Transport error | Leave file `pending`; retry next cycle |

**Planned migration:** Phase 5.5 will switch to gRPC `ScanBatch` with
native `bytes` payloads (no base64).

### 13.6 `grpc_transport`

**Purpose:** Long-lived server-push channel and graceful shutdown
notification.

**Implementation:** `grpc::ClientReaderWriter` on the `AgentService::Channel`
bidirectional streaming RPC.

**Lifecycle:**

1. `initialize` reads `server.base_url`, `grpc_port`, `grpc_tls`,
   `grpc_ca_path`, and `api_token` from config. Derives the gRPC target
   from `base_url` and `grpc_port`.
2. `start` spawns the manager thread.
3. Manager thread polls `state` for `agent_id` every 1 second.
4. Once enrolled, creates a `grpc::Channel` once:
   - `grpc_tls=true` — `grpc::SslCredentials` with `pem_root_certs`
     loaded from `grpc_ca_path` if set
   - `grpc_tls=false` — `grpc::InsecureChannelCredentials` (dev only)
5. Creates an `AgentService::Stub` over the channel.
6. Opens the `Channel` bidi stream with metadata
   `authorization: Bearer <api_token>` and `x-agent-id: <agent_id>`.
7. Loops on `stream->Read(&resp)` and dispatches messages.
8. On disconnect, applies exponential backoff with 20 % jitter, base
   `reconnect_backoff_seconds`, cap `reconnect_backoff_max_seconds`.

**Server-to-agent messages (`ChannelResponse` oneof):**

| Message | Action |
| --- | --- |
| `ServerPolicyUpdate` | Log version. Full policy-apply logic is deferred to the policy engine (Phase 2). |
| `ServerCommand` | Log `command_type`. Full command handling is deferred (Phase 2, milestone 22). |
| `ServerRevoke` | Map reason: `TOKEN_REVOKED` to `AuthRevoked`, `AGENT_REVOKED` to `AgentRevoked`. Request shutdown. |

**Agent-to-server messages:** `ClientShutdown` on graceful stop, carrying
the mapped `ShutdownReason`.

**Status code handling:**

| gRPC status | Behavior |
| --- | --- |
| `OK` | Clean exit (agent-initiated stop) |
| `UNAVAILABLE` | Reconnect with backoff |
| `DEADLINE_EXCEEDED` | Reconnect with backoff |
| `RESOURCE_EXHAUSTED` | Reconnect with backoff |
| `INTERNAL` | Reconnect with backoff |
| `CANCELLED` (when not stopping) | Reconnect with backoff |
| `UNAUTHENTICATED` | `ShutdownController::request(AuthRevoked)` — no reconnect |
| `PERMISSION_DENIED` | `ShutdownController::request(AgentRevoked)` — no reconnect |

`UNIMPLEMENTED` is not yet wired — it should trigger
`ShutdownController::request(FatalError)` (tracked for Phase 5.3c).

---

## 14. Data Flow

### 14.1 Inventory to verdict

```
1. Inventory walks the filesystem
   └── Writes metadata + hash, scan_status = 'pending'
       (only for files that pass eligibility: regular file,
        whitelisted extension, size below limit)

2. Content scanner queries pending files
   └── Reads content from disk, computes SHA-256
       └── If hash was previously scanned: reuse cached verdict,
           no HTTP call
       └── Otherwise:
           ├── Splits into overlapping chunks
           ├── Batches chunks up to batch_size / batch_max_bytes
           ├── POST /api/v1/agent/scan/batch
           └── Merges per-chunk verdicts

3. Verdict written back to `files` table
   └── is_sensitive, sensitivity, scan_status = 'scanned'

4. Enforcement (future) queries is_sensitive = 1 files
```

### 14.2 Enrollment to heartbeat

```
1. agent_sync starts
   └── Reads agent_id from `state` table
       ├── If present: sends immediate validation heartbeat
       │   └── 401 → ShutdownController::request(AuthRevoked)
       │   └── 403 → ShutdownController::request(AgentRevoked)
       │   └── 200 → enter heartbeat loop
       └── If absent: POST /api/v1/agent/register
           └── Persists returned agent_id to `state`
           └── Enters heartbeat loop

2. grpc_transport starts
   └── Polls `state` for agent_id every 1 s
       └── Once present: opens Channel bidi stream
           └── Reads server-pushed messages
           └── Sends ClientShutdown on stop
```

### 14.3 Shutdown

```
Any source:
  - SIGTERM / Ctrl+C → ShutdownController::request(UserRequested)
  - Service control stop → ShutdownController::request(UserRequested)
  - HTTP 401 → ShutdownController::request(AuthRevoked)
  - HTTP 403 → ShutdownController::request(AgentRevoked)
  - gRPC UNAUTHENTICATED → ShutdownController::request(AuthRevoked)
  - gRPC PERMISSION_DENIED → ShutdownController::request(AgentRevoked)
  - gRPC ServerRevoke → ShutdownController::request(...)
  - Any module fatal error → ShutdownController::request(FatalError)

ShutdownController:
  - First request wins; subsequent requests are no-ops
  - Wakes all waiters

Service main loop:
  - Unblocks from ShutdownController::wait()
  - Calls ModuleManager::stop_all() in reverse order
  - Calls ModuleManager::shutdown_all() in reverse order
  - grpc_transport sends ClientShutdown with the mapped reason
  - Returns rc=0

Application:
  - Logs "Service returned rc=0 (reason=<reason>)"
  - Destroys members in reverse dependency order
  - Shuts down the logger last
```

---

## 15. TLS and Security

### 15.1 Threat model

Every outbound connection carries the `axdlp_user_...` bearer token. If
this token is intercepted, an attacker can impersonate the agent against
the Core server, receiving policy updates, commands, and revocations that
were intended for the legitimate agent. For this reason, TLS is
mandatory and never optional.

### 15.2 Transport security

- **HTTP** — cpp-httplib with
  `cli.enable_server_certificate_verification(true)` set unconditionally
  on every `https://` call. Certificate chain and hostname are both
  verified.
- **gRPC** — `grpc::SslCredentials` with either the system trust store
  or a custom PEM bundle (`server.grpc_ca_path`). The gRPC runtime
  verifies chain and hostname.

### 15.3 No verification opt-out

The `verify_tls` field has been removed from every layer:

- Not parsed in `Config`
- Not passed to `HttpClient`
- Not accepted by `WebSocketClient` (which itself has been removed)
- Not present in `agent.schema.json`

Legacy config files that contain `verify_tls` produce a warning that the
field is ignored. Verification still happens.

### 15.4 Certificate and hostname verification details

- SNI is set before every TLS handshake on both HTTP and gRPC.
- Hostname verification uses `X509_VERIFY_PARAM_set1_host` on the HTTP
  path (via OpenSSL's cpp-httplib integration) and gRPC's built-in
  name verification on the gRPC path.

### 15.5 Static linking

Both platforms link OpenSSL and gRPC statically. Verification:

```bash
ldd build/linux-x86_64-release/axiomdlp-agent | grep -E 'ssl|grpc'
# (empty)
```

```powershell
dumpbin /dependents build\windows-x64-msvc-release\axiomdlp-agent.exe
# No libssl-*.dll, no libcrypto-*.dll, no grpc-*.dll
```

### 15.6 Token handling

- The token is read from `server.api_token` at startup.
- If it is empty, `agent_sync` and `content_scanner` fail initialization
  with the message "no valid API token".
- The token is never written to disk by the agent; it lives only in the
  config file, which the operator is responsible for protecting.
- The token is never logged.

---

## 16. Shutdown Model

### 16.1 The single authority

`ShutdownController` (in `src/foundation/shutdown.h`) is the only
component that can initiate a shutdown. It exposes:

- `request(ShutdownReason)` — first caller wins, subsequent calls are
  no-ops
- `wait()` — blocks until a request arrives
- `wait_for(duration)` — bounded wait
- `is_requested()` — non-blocking check
- `reason()` — current reason (or `None`)

### 16.2 Shutdown reasons

| Reason | Cause |
| --- | --- |
| `None` | Initial state — not shutting down |
| `UserRequested` | Ctrl+C, SIGTERM, systemctl stop, SCM stop |
| `AuthRevoked` | HTTP 401, gRPC `UNAUTHENTICATED`, or `ServerRevoke{TOKEN_REVOKED}` |
| `AgentRevoked` | HTTP 403, gRPC `PERMISSION_DENIED`, or `ServerRevoke{AGENT_REVOKED}` |
| `ConfigError` | Fatal configuration error at startup |
| `FatalError` | Unrecoverable module failure |
| `ServerShutdown` | Server-side shutdown notice |

### 16.3 Propagation

Once requested, every module sees the reason via
`ModuleContext::shutdown.reason()` and can act accordingly. The final
log line always reports the reason:

```
Service returned rc=0 (reason=user_requested)
```

The gRPC transport sends a `ClientShutdown` message with the same reason
before closing the stream, so the Core server also sees the reason in its
logs.

---

## 17. Testing

### 17.1 Manual smoke test

Prerequisites: a running AxiomDLP Core server on `http://127.0.0.1:3000`
(HTTP) and `grpc://127.0.0.1:50051` (gRPC) with a valid
`axdlp_user_...` token.

Linux:

```bash
# Terminal 1: start the Core
# (refer to the Core server repository for startup instructions)

# Terminal 2: start the agent
./build/linux-x86_64-release/axiomdlp-agent \
    --foreground \
    --config config/agent.default.json
```

Expected log output:

```
[info] AxiomDLP Agent v0.1.0
[info] Config loaded from: config/agent.default.json
[info] Database: opened 'data/agent.db'
[info] Migrations: schema version 4 -> 4
[info] ModuleManager: registering module 'filesystem_watcher' v1.0.0
[info] ModuleManager: registering module 'inventory' v1.0.0
[info] ModuleManager: registering module 'event_logger' v1.0.0
[info] ModuleManager: registering module 'grpc_transport' v1.0.0
[info] ModuleManager: registering module 'agent_sync' v1.0.0
[info] ModuleManager: registering module 'content_scanner' v1.0.0
[info] agent_sync: enrolled (agent_id=1, user_id=1, org_id=2)
[info] grpc_transport: opening Channel bidi stream...
[info] grpc_transport: state = connected
```

Press Ctrl+C. Expected shutdown output:

```
[info] axiomdlp-agent received stop request (reason=user_requested)
[info] ModuleManager: stopping 6 module(s)
[info] grpc_transport: sending graceful goodbye...
[info] grpc_transport: stream disconnected (OK)
[info] Service returned rc=0 (reason=user_requested)
[info] Application shutting down
```

### 17.2 Automation roadmap

A comprehensive automated test suite is planned for Phase 6 of the
migration roadmap. It will include:

- **Unit tests** — C++ test binaries for the policy decision table,
  status-code mapping, backoff bounds, outbound queue backpressure,
  and base64 usage rules
- **Integration tests** — end-to-end against a live Core server with
  both real and mocked transports
- **Load tests** — 10,000 simulated agents against one Core instance
- **Chaos tests** — kill the Core mid-scan, network partition, corrupt
  frames, disk full
- **TLS tests** — wrong CA, wrong hostname, expired cert, plaintext
  attempt on TLS port
- **Wire format tests** — confirm base64 is absent from `ScanBatch`
  payloads; measure wire-size reduction

This roadmap is fully specified in the migration TODO and is not yet
implemented.

---

## 18. Roadmap

### 18.1 Phase 5 — gRPC migration (in progress)

Completed:

- gRPC and Protobuf as mandatory build dependencies
- Removal of WebSocket, Boost, and Boost.Beast
- `HttpClient` rewritten on top of cpp-httplib
- `grpc_transport` module with bidirectional Channel stream
- Status-code-to-shutdown mapping on the gRPC path

Remaining:

- 5.4 — `agent_sync` on gRPC unary (`Register`, `Heartbeat`)
- 5.5 — `content_scanner` on gRPC `ScanBatch` (native bytes, no base64)
- 5.7 — unify server-pushed message dispatch (typed handler signatures
  preferred over JSON adapter)
- 5.8 — `server.transport` config selector (`grpc` | `websocket`), with
  `grpc` becoming the default in Phase 7
- 5.3c — wire `UNIMPLEMENTED` gRPC status to `FatalError`

### 18.2 Phase 6 — testing and hardening

Unit, integration, load, chaos, TLS, and wire-format regression tests on
all four targets and both transports (during the transition period).
Observability baseline (server-side gRPC metrics, structured logs on
both sides). Security review pass. Full specification is in the
migration TODO.

### 18.3 Phase 7 — cutover and deletion

- Flip `server.transport` default to `grpc`
- Ship one release cycle with `grpc` as the default while keeping the
  HTTP fallback compiled in
- After one clean release cycle, delete all remaining HTTP transport code
  from both the agent and the Core
- Rewrite or delete stale documentation
- Confirm no WebSocket or HTTP transport code remains in either
  repository

### 18.4 Phase 2 (product) — see `next.md`

Milestones 15 through 25:

- Policy engine
- User-mode enforcement (file, clipboard, print)
- Extended content extraction (PDF, DOCX, XLSX, OCR)
- Advanced detection (EDM, PDM, document registry)
- IPC and tray helper
- Kernel enforcement (Linux eBPF, Windows Minifilter)
- Remote commands and auto-update
- Platform expansion (macOS, ARM64)
- Observability and telemetry
- Hardening and tamper resistance

---

## 19. Known Issues and Limitations

### 19.1 `docs/file_structure.md` is stale

The file describes an architecture that does not exist in the current
source tree (`src/core/`, `src/interfaces/`, `kernel/`, `helper/`,
`external/`, `tests/unit/`, `tests/integration/`). It must be rewritten
against the actual tree or deleted. Tracked as Phase 7.5.

### 19.2 `src/net/outbound_queue.{h,cpp}` is dead code

The file is compiled and linked but no module references it after the
WebSocket removal. It is a candidate for deletion or for repurposing as
a future gRPC send-queue.

### 19.3 `grpc_transport::health()` reports degraded

The module returns
`degraded("stub — RPC implementation pending")` even though the Channel
bidi stream is implemented and functioning. This is conservative: the
module has not been load-tested, and its message dispatch is inline
rather than going through a unified router.

### 19.4 `config/agent.schema.json` describes an aspirational state

The schema description says "Uses gRPC exclusively" but `agent_sync`
and `content_scanner` still use HTTP. The schema should be updated when
Phase 5 finishes.

### 19.5 Windows path case sensitivity

The glob matcher used for exclusions is case-sensitive. NTFS is
case-insensitive. This can cause exclusions to miss when the OS reports
a path in a different case than the pattern. The workaround is to match
what the OS reports, but this is fragile. Tracked for future
improvement.

### 19.6 Windows Ctrl+C not handled in foreground mode

No `SetConsoleCtrlHandler` is registered on the Windows side.
`taskkill /PID` terminates the process without graceful shutdown. The
SCM path handles stop correctly; only the foreground path lacks graceful
handling.

### 19.7 `ModuleManager` does not skip failed modules

A module that returns `false` from `initialize()` still has `start()`
called on it. This has not caused problems because failures are rare
and the affected modules are fail-safe, but it should be fixed for
correctness.

### 19.8 `dev` and `inode` are placeholders

`dev` is always 0 and `inode` is a hash of the file path. This is
sufficient for uniqueness in the current schema, but it is not a real
inode reference. A future revision may need to read the actual device
and inode numbers from `stat()`.

### 19.9 `walks` table is unused

The schema exists (for scan history) but nothing populates it. Reserved
for future use.

### 19.10 Content scanner only handles text files

PDF, DOCX, XLSX, and images are marked `not_applicable`. Rich-document
extraction is a Phase 2 milestone.

### 19.11 No process or user attribution

Events carry a path but not the process or user that caused the
operation. This requires fanotify on Linux and a Minifilter on Windows,
both of which are Phase 2 milestones.

### 19.12 Windows binary sizes need re-measuring

The previously verified sizes (8.61 MB x64, 6.37 MB x86) predate the
mandatory gRPC and protobuf dependency. Current sizes will be larger.
Re-measure after a clean Windows build and update this document.

### 19.13 No `server.transport` selector

The agent runs the gRPC Channel transport and the HTTP sync path
simultaneously. This works because the two paths are independent, but
it is not what the final architecture will look like. The selector is a
Phase 5.8 item.

### 19.14 Windows setup requires manual workarounds on slow networks

The `setup-windows-msvc.ps1` fast-mode uses `Invoke-WebRequest`, which
can stall on slow GitHub CDN links. The documented fallback (aria2c in
WSL, then copy into `%USERPROFILE%\vcpkg\downloads\`) works but is not
scripted. Future improvement: switch to `Start-BitsTransfer`.

### 19.15 Linux OpenSSL and gRPC build times

Building OpenSSL 3.3.2 and gRPC 1.62.0 for both Linux targets takes
30–60 minutes on first run. This is a one-time cost per version bump.
Cached trees under `.deps/` are reused across subsequent builds.

---

## 20. Contributing

### 20.1 General principles

- Ship small. Every change must build on all four targets before the
  next change begins.
- Verify on Linux x86_64, Linux i686, Windows x64, and Windows x86
  before merging.
- Update `done.md` when a milestone or bug fix is complete.
- Update `next.md` when a roadmap item changes.
- Keep `docs/` accurate — if the source tree changes, the docs must
  change too. Stale documentation is treated as a bug.

### 20.2 Code style

- `clang-format` with the repository `.clang-format` file
- `clang-tidy` with the repository `.clang-tidy` file
- C++20 as the target standard
- RAII for all resources
- Exceptions are caught at module boundaries, not used for control flow

### 20.3 Testing requirements

- New module: at least a smoke test in the manual smoke-test document
- Bug fix: reproduce the bug first, then fix, then verify the fix
  resolves the reproduction
- New dependency: verify it builds on all four targets and does not
  introduce shared library dependencies

### 20.4 Documentation requirements

- New module: add a subsection under section 13 of this README
- New config field: update section 10 and `config/agent.schema.json`
- New dependency: update section 6 and the appropriate platform section
- Bug fix that required non-trivial debugging: add an entry under the
  appropriate section in `done.md`

### 20.5 Commit messages

Descriptive commit messages that explain what changed and why. Reference
specific bug identifiers (for example `W22`, `G4`) when applicable.

---

## 21. Related Documents

- **`done.md`** — Complete implementation log, bug history, and lessons
  learned. Includes all fixed bugs from the initial design through the
  Windows MSVC migration and the Phase 5 gRPC migration.
- **`next.md`** — Phase 2 product roadmap with milestones 15 through 25
  covering the policy engine, enforcement, extraction, IPC, kernel
  hooks, remote commands, platform expansion, observability, and
  hardening.
- **`docs/file_structure.md`** — Currently stale (describes an
  architecture from an earlier design). Do not rely on it. A rewrite is
  tracked in Phase 7.5 of the migration roadmap.
- **`config/agent.schema.json`** — Machine-readable JSON schema for the
  configuration file. Authoritative for field types, defaults, and
  allowed values.
- **`src/features/grpc_transport/agent.proto`** — Protocol buffer
  definition for the agent-to-Core API. Must be byte-identical to the
  matching file in the Core repository; a CI check for this is planned
  for Phase 6.

---

**AxiomDLP Endpoint Agent** is designed to be a long-lived, extendable
foundation. Every capability added from here on slots into the module
framework without touching the core, and every wire change goes through
`agent.proto` so both sides stay in sync. The current release is a
working observe-and-report layer; the enforcement layer on top of it is
the next product milestone.