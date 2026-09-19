# AxiomDLP Endpoint Agent — README

A cross-platform Data Leak Prevention (DLP) endpoint agent written in modern C++. Runs as a native background service on Linux and Windows. Builds four targets from a single Linux host.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Project Status](#2-project-status)
3. [Architecture](#3-architecture)
4. [Directory Structure](#4-directory-structure)
5. [Build System](#5-build-system)
6. [Dependencies](#6-dependencies)
7. [Configuration](#7-configuration)
8. [Running as a Service](#8-running-as-a-service)
9. [Platform-Specific Details](#9-platform-specific-details)
10. [Verified Behavior](#10-verified-behavior)
11. [Known Issues](#11-known-issues)
12. [Development Workflow](#12-development-workflow)
13. [Roadmap](#13-roadmap)
14. [License](#14-license)

---

## 1. Overview

**AxiomDLP Endpoint Agent** is the endpoint-side component of the AxiomDLP platform. It is designed to:

- Run as a privileged background service on user endpoints
- Monitor local activity (filesystem, USB, clipboard, printing, browser)
- Detect sensitive data in real time
- Enforce DLP policies locally
- Work fully offline with a local cache
- Report events to the AxiomDLP Core backend
- Complement the existing email milter (which handles SMTP egress)

**Current state:** The foundational infrastructure is complete. The agent installs, runs, and stops as a real system service on both Linux and Windows. It does not yet perform any DLP work — it is the platform that future DLP features will be built on.

---

## 2. Project Status

### Completed

| Component | Status |
|-----------|--------|
| Cross-compilation (4 targets from one Linux host) | ✅ Complete |
| Async structured logging (spdlog) | ✅ Complete |
| Configuration loading (JSON with validation) | ✅ Complete |
| Linux service lifecycle (systemd) | ✅ Complete & verified |
| Windows service lifecycle (SCM) | ✅ Complete & verified |
| Graceful shutdown (signals / SCM controls) | ✅ Complete |
| Install / uninstall with full sequence | ✅ Complete |
| Safe subprocess spawning (no shell) | ✅ Complete |
| Platform abstraction layer | ✅ Complete |

### Not Yet Implemented

| Component | Milestone |
|-----------|-----------|
| Module system (plugin architecture) | 5 |
| Event bus | 6 |
| Filesystem monitoring | 7 |
| Local regex scanner | 8 |
| Policy engine | 9 |
| SQLite storage | 10 |
| User-mode enforcement | 11 |
| IPC + user helper (tray) | 12 |
| Transport to core (HTTP + WebSocket) | 13 |
| Kernel enforcement (minifilter / eBPF) | 14 |

---

## 3. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                            │
│                   Thin entry point (5 lines)                │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                      Application                            │
│  - Owns Config                                              │
│  - Owns Logger                                              │
│  - Owns IService (platform-specific)                        │
│  - Parses args, dispatches install/uninstall/run            │
└──────────────────────────┬──────────────────────────────────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          ▼                ▼                ▼
    ┌─────────┐      ┌─────────┐      ┌──────────┐
    │ Config  │      │ Logger  │      │ IService │
    │ (JSON)  │      │ spdlog  │      │ abstract │
    └─────────┘      └─────────┘      └────┬─────┘
                                            │
                              ┌─────────────┴─────────────┐
                              ▼                           ▼
                    ┌───────────────────┐      ┌───────────────────┐
                    │   LinuxService    │      │  WindowsService   │
                    │   systemd         │      │  SCM              │
                    │   sd_notify       │      │  Control Handler  │
                    │   SIGTERM/SIGINT  │      │  SERVICE_CONTROL  │
                    └───────────────────┘      └───────────────────┘
```

### Design Principles

1. **Interface in `core/`, implementation in `platform/<os>/`.** Business logic never includes OS headers.
2. **CMake selects one platform file per target.** No runtime dispatch, no `#ifdef` in application code.
3. **`#ifdef HAVE_X` guards optional features.** Same source file compiles for every target with different feature sets.
4. **Fail-safe configuration.** Missing or invalid config produces warnings, not crashes.
5. **RAII guarantees cleanup.** Destructors flush logging and release resources, even on error paths.

---

## 4. Directory Structure

```
axiomdlp-agent/
├── CMakeLists.txt                      Root build recipe
├── CMakePresets.json                   8 named build configurations
├── .gitignore
├── LICENSE
├── README.md
│
├── .deps/                              Cached third-party sources
│   ├── spdlog-src/
│   └── nlohmann_json-src/
│
├── cmake/
│   └── toolchains/                     Cross-compile toolchain files
│       ├── linux-x86_64.cmake
│       ├── linux-i686.cmake
│       ├── windows-x86_64.cmake
│       └── windows-i686.cmake
│
├── config/
│   ├── agent.default.json              Runtime configuration
│   └── agent.schema.json               JSON Schema for editors
│
├── docs/                               Architecture documents
│   ├── architecture.md
│   ├── event_schema.md
│   ├── ipc_protocol.md
│   ├── modules.md
│   └── policy_format.md
│
├── packaging/
│   ├── linux/systemd/                  Reference systemd unit
│   └── windows/                        Reference PowerShell install
│
├── scripts/
│   └── build-all.sh                    Build all four targets
│
└── src/
    ├── main.cpp                        Entry point (5 lines)
    │
    ├── core/                           Framework (no OS-specific code)
    │   ├── application.{h,cpp}         Orchestration
    │   ├── config.{h,cpp}              JSON config loading
    │   ├── logger.{h,cpp}              Async logging wrapper
    │   ├── paths.h                     Platform path API
    │   └── service.h                   IService interface
    │
    └── platform/                       OS-specific implementations
        ├── linux/
        │   ├── paths_linux.cpp         FHS paths
        │   └── service_linux.cpp       systemd + sd_notify
        └── windows/
            ├── paths_windows.cpp       ProgramData paths
            └── service_win.cpp         SCM + ServiceMain
```

---

## 5. Build System

### 5.1 Four Targets

From one Linux host, four binaries are produced:

| Preset | Output | Purpose |
|--------|--------|---------|
| `linux-x86_64-{debug,release}` | `build/linux-x86_64-*/axiomdlp-agent` | Modern Linux |
| `linux-i686-{debug,release}` | `build/linux-i686-*/axiomdlp-agent` | Legacy 32-bit Linux |
| `windows-x86_64-{debug,release}` | `build/windows-x86_64-*/axiomdlp-agent.exe` | Modern Windows |
| `windows-i686-{debug,release}` | `build/windows-i686-*/axiomdlp-agent.exe` | Legacy 32-bit Windows |

### 5.2 Toolchain Files

Each target has a toolchain file in `cmake/toolchains/`.

**Linux toolchains** use the host's native `gcc` / `g++`. The 32-bit Linux toolchain adds `-m32` flags.

**Windows toolchains** use MinGW-w64 cross-compilers with strict root path isolation:

```cmake
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # Host tools
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # MinGW libraries only
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # MinGW headers only
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)    # MinGW packages only
```

These four lines prevent cross-compilation contamination — a Linux `.so` can never accidentally link into a Windows `.exe`.

### 5.3 Dependency Caching

Third-party sources are cached in `.deps/` to avoid re-downloading after `rm -rf build`.

| Dependency | Source | Reason |
|------------|--------|--------|
| **spdlog** | FetchContent with `.deps/spdlog-src/` cache | Compiled library — must be built per target |
| **nlohmann_json** | System package (Linux) / `.deps/nlohmann_json-src/` (Windows) | Header-only, but cross-compile needs isolated headers |

### 5.4 Prerequisites

On Kali/Debian/Ubuntu:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y \
    build-essential \
    gcc-multilib g++-multilib \
    mingw-w64 \
    ninja-build \
    git \
    file \
    libsystemd-dev libsystemd-dev:i386 \
    libspdlog-dev nlohmann-json3-dev
```

### 5.5 Building

**All targets:**

```bash
./scripts/build-all.sh
```

First run: ~2 minutes (includes one-time dependency clone). Subsequent runs: ~1 minute.

**Single target:**

```bash
cmake --preset linux-x86_64-release
cmake --build --preset linux-x86_64-release
```

**Clean rebuild:**

```bash
rm -rf build
./scripts/build-all.sh
```

The `.deps/` cache survives — no re-downloads.

---

## 6. Dependencies

### Runtime

| Library | Purpose | How It's Used |
|---------|---------|---------------|
| **spdlog** | Async logging | FetchContent with `.deps/` cache |
| **nlohmann/json** | JSON parsing | System on Linux; FetchContent on Windows |
| **libsystemd** | `sd_notify` | Detected via pkg-config (Linux only) |
| **fmt** | String formatting | Transitive dependency of spdlog |

### Build-Time

| Tool | Purpose |
|------|---------|
| **CMake** ≥ 3.24 | Build system |
| **Ninja** | Build generator |
| **GCC 13+** | Linux compilation |
| **MinGW-w64** | Windows cross-compilation |
| **pkg-config** | Library detection |

### Windows Runtime

Windows binaries are **fully static** — no MSVC redistributable, no `libstdc++-6.dll`, no `libgcc_s_seh-1.dll`. The `.exe` runs on any Windows 7+ system with no installation.

---

## 7. Configuration

### 7.1 File Location

The agent loads `config/agent.default.json` relative to its working directory.

**Default locations (planned):**
- Linux: `/etc/axiomdlp/agent.json`
- Windows: `C:\ProgramData\AxiomDLP\agent.json`

### 7.2 Current Schema

```json
{
  "logging": {
    "dir":   "logs",
    "file":  "agent.log",
    "level": "info"
  }
}
```

### 7.3 Field Reference

| Field | Type | Default | Allowed Values |
|-------|------|---------|----------------|
| `logging.dir` | string | `"logs"` | Any directory path |
| `logging.file` | string | `"agent.log"` | Any filename |
| `logging.level` | string | `"info"` | `trace`, `debug`, `info`, `warn`, `error`, `critical`, `off` |

### 7.4 Behavior

| Scenario | Behavior |
|----------|----------|
| Missing file | Warning, use defaults |
| Invalid JSON | Warning, use defaults |
| Wrong field type | Warning, use default for that field |
| Invalid level | Warning, use `"info"` |
| Missing field | Silently use default |

**The agent always starts.** Config problems produce warnings, not failures.

### 7.5 Schema File

`config/agent.schema.json` is a JSON Schema that describes the config format. It is used by:

- **Editors** (VS Code, CLion) for autocomplete
- **CI pipelines** for validation
- **Documentation** as a single source of truth

It is **not** used at runtime — the C++ code validates independently.

---

## 8. Running as a Service

### 8.1 Linux (systemd)

**Install** (requires root):

```bash
sudo ./build/linux-x86_64-release/axiomdlp-agent --install
```

This performs the full sequence:

1. Write `/etc/systemd/system/axiomdlp-agent.service`
2. `systemctl daemon-reload`
3. `systemctl enable --now axiomdlp-agent`

**Expected output:**

```
[info] installed unit file: /etc/systemd/system/axiomdlp-agent.service
[info] reloaded systemd configuration
[info] enabled and started service axiomdlp-agent
```

The service is running immediately. No further commands needed.

**Verify:**

```bash
sudo systemctl status axiomdlp-agent
```

Should show `Active: active (running)`.

**View logs:**

```bash
sudo journalctl -u axiomdlp-agent -f
```

**Stop / start / restart:**

```bash
sudo systemctl stop axiomdlp-agent
sudo systemctl start axiomdlp-agent
sudo systemctl restart axiomdlp-agent
```

**Uninstall:**

```bash
sudo ./build/linux-x86_64-release/axiomdlp-agent --uninstall
```

Full sequence:

1. `systemctl stop axiomdlp-agent`
2. `systemctl disable axiomdlp-agent`
3. Remove the unit file
4. `systemctl daemon-reload`

**Expected output:**

```
[info] stopping service axiomdlp-agent
[info]   stop: ok
[info] disabling service axiomdlp-agent
[info]   disable: ok
[info] removed unit file: /etc/systemd/system/axiomdlp-agent.service
[info] reloaded systemd configuration
[info] uninstalled service axiomdlp-agent
```

### 8.2 Windows (SCM)

**Install** (requires Administrator Command Prompt):

```cmd
axiomdlp-agent.exe --install
```

This performs:

1. `CreateServiceA` — register with SCM (auto-start)
2. `StartServiceA` — start immediately

**Expected output:**

```
[info] service AxiomDLPAgent installed
[info] started service AxiomDLPAgent
```

**Verify:**

```cmd
sc query AxiomDLPAgent
```

Should show `STATE : 4 RUNNING`.

Or open `services.msc` — the service appears as **AxiomDLPAgent** with Status **Running**.

**Uninstall:**

```cmd
axiomdlp-agent.exe --uninstall
```

Full sequence:

1. `ControlService(STOP)` + wait for stop
2. `DeleteService`

**Expected output:**

```
[info] stop requested for AxiomDLPAgent
[info] service AxiomDLPAgent stopped
[info] service AxiomDLPAgent uninstalled
```

### 8.3 Foreground Mode (Development)

To run the agent in the current terminal instead of as a service:

```bash
./build/linux-x86_64-release/axiomdlp-agent --foreground
```

Or on Windows:

```cmd
axiomdlp-agent.exe --foreground
```

The agent runs identically, but logs to the terminal. Press `Ctrl+C` to stop.

**Note:** On Windows, Ctrl+C handling is not yet implemented — use Task Manager to kill the process, or close the terminal.

---

## 9. Platform-Specific Details

### 9.1 Linux — systemd

| Aspect | Detail |
|--------|--------|
| Service manager | systemd |
| Unit file | `/etc/systemd/system/axiomdlp-agent.service` |
| Type | `notify` (waits for `sd_notify(READY=1)`) |
| Stop signal | `SIGTERM` |
| Stop handler | Custom signal handler sets atomic flag |
| Ready signal | `sd_notify(0, "READY=1")` |
| Restart policy | `Restart=on-failure`, `RestartSec=5` |
| Logs | journald (`StandardOutput=journal`) |
| Install directory | `/etc/systemd/system/` |
| Default service name | `axiomdlp-agent` (lowercase-with-hyphens) |

**Subprocess spawning:** Uses `fork` + `execvp` — no shell involved. Shell injection is impossible.

### 9.2 Windows — SCM

| Aspect | Detail |
|--------|--------|
| Service manager | Service Control Manager (SCM) |
| Service entry | Windows Registry (managed by SCM) |
| Service type | `SERVICE_WIN32_OWN_PROCESS` |
| Startup type | `SERVICE_AUTO_START` |
| Stop code | `SERVICE_CONTROL_STOP` |
| Stop handler | Registered control handler function |
| Ready report | `SetServiceStatus(SERVICE_RUNNING)` |
| Recovery | None configured (default SCM behavior) |
| Logs | File only (no `journald` equivalent) |
| Default service name | `AxiomDLPAgent` (PascalCase) |
| Account | LocalSystem (default) |
| Session | Session 0 (isolated service session) |

**Two-thread model:** Windows services use two threads:

- **Main thread** — sits in `StartServiceCtrlDispatcher`, blocks until the service stops
- **Service thread** — created by the SCM, runs `service_main` and the main loop

**Subprocess spawning:** Uses Win32 API directly. No shell involved anywhere.

---

## 10. Verified Behavior

Both platforms have been verified through live testing.

### 10.1 Linux Verification

| Test | Result |
|------|--------|
| Binary runs in foreground | ✅ Confirmed |
| Binary installs as systemd service | ✅ Confirmed |
| Service starts automatically after install | ✅ Confirmed |
| Service runs in background | ✅ Confirmed (via `systemctl status`) |
| `sd_notify(READY=1)` reaches systemd | ✅ Confirmed |
| Logs go to journald | ✅ Confirmed |
| `systemctl stop` triggers graceful shutdown | ✅ Confirmed |
| Uninstall stops, disables, and deletes | ✅ Confirmed |
| Idempotent reinstall | ✅ Confirmed |
| No orphaned processes after uninstall | ✅ Confirmed |
| 32-bit binary works | ✅ Confirmed |

### 10.2 Windows Verification

| Test | Result |
|------|--------|
| Binary runs on Windows 10 | ✅ Confirmed |
| Foreground auto-fallback works | ✅ Confirmed |
| `--foreground` flag works | ✅ Confirmed |
| Install from Administrator | ✅ Confirmed |
| Service runs under SCM | ✅ Confirmed (Session 0) |
| Two-thread model active | ✅ Confirmed (log shows two thread IDs) |
| Log says "Windows service mode" | ✅ Confirmed |
| `--uninstall` stops and deletes | ✅ Confirmed |
| Idempotent reinstall | ✅ Confirmed |
| Static binary (no DLLs) | ✅ Confirmed |

### 10.3 Sample Windows Service Log

```
[01:22:35.404] [t:2744] AxiomDLP Agent v0.1.0
[01:22:35.404] [t:2744] Config loaded from: config/agent.default.json
[01:22:35.404] [t:2744] Config: config file not found...
[01:22:35.407] [t:3424] AxiomDLPAgent running (Windows service mode)   ← different thread!
[01:22:45.088] [t:3424] AxiomDLPAgent stopping (Windows service mode)
[01:22:45.090] [t:2744] Service returned rc=0
[01:22:45.090] [t:2744] Application shutting down
```

The two different thread IDs (`2744` and `3424`) prove the SCM's two-thread service model is active. This is the definitive confirmation that the agent runs as a real Windows service.

---

## 11. Known Issues

### 11.1 Config File Not Found When Running as Service

**Symptom:**

```
[warning] Config: config file not found at 'config/agent.default.json'; using built-in defaults
```

**Cause:** The service's working directory is not the project root:

- Linux: systemd starts services from `/`
- Windows: the SCM starts services from `C:\Windows\System32`

The relative path `config/agent.default.json` doesn't resolve in those directories.

**Impact:** The agent uses built-in defaults. It works — but custom config isn't loaded.

**Planned fix:** Add a config path search that tries multiple locations:

1. `--config <path>` command-line argument
2. `AXIOM_CONFIG_PATH` environment variable
3. Working directory: `./config/agent.json`
4. Executable directory: `<exe_dir>/config/agent.json`
5. Platform default: `/etc/axiomdlp/agent.json` (Linux) or `C:\ProgramData\AxiomDLP\agent.json` (Windows)

### 11.2 No Ctrl+C Handler in Windows Foreground Mode

**Symptom:** Pressing Ctrl+C in a Windows Command Prompt doesn't stop the agent cleanly. The process must be killed via Task Manager.

**Cause:** Windows doesn't use POSIX signals. It uses console control handlers, which aren't registered.

**Impact:** Only affects foreground mode. The service mode (SCM-driven) stops cleanly via `SERVICE_CONTROL_STOP`.

**Planned fix:** Add a `SetConsoleCtrlHandler` callback that sets the stop flag.

### 11.3 systemd Notification During Stop (Linux)

**Symptom (harmless):**

```
systemd[1]: Got notification message from PID XXXX, but reception is disabled
```

**Cause:** A `sd_notify(STOPPING=1)` call arrives after systemd has already stopped listening for notifications.

**Impact:** Cosmetic log line only. Shutdown succeeds either way.

**Planned fix:** Optional — remove the `STOPPING=1` call, or accept the harmless warning.

---

## 12. Development Workflow

### 12.1 Typical Iteration

```bash
# Edit code
$EDITOR src/core/logger.cpp

# Build one target (fast)
cmake --build --preset linux-x86_64-debug

# Run in foreground
./build/linux-x86_64-debug/axiomdlp-agent --foreground
```

### 12.2 Full Cross-Platform Check

Before committing, verify all four targets build:

```bash
./scripts/build-all.sh
```

### 12.3 Editor Setup (VS Code)

Create `.vscode/c_cpp_properties.json`:

```json
{
  "version": 4,
  "configurations": [
    {
      "name": "Linux",
      "compileCommands": "${workspaceFolder}/build/linux-x86_64-debug/compile_commands.json",
      "intelliSenseMode": "linux-gcc-x64"
    },
    {
      "name": "Windows (MinGW)",
      "compileCommands": "${workspaceFolder}/build/windows-x86_64-debug/compile_commands.json",
      "intelliSenseMode": "windows-gcc-x64"
    }
  ]
}
```

Switch between configs from the VS Code status bar (bottom-right).

### 12.4 Common Commands

| Task | Command |
|------|---------|
| Build all targets | `./scripts/build-all.sh` |
| Build one target | `cmake --build --preset linux-x86_64-release` |
| Run in foreground | `./build/linux-x86_64-release/axiomdlp-agent --foreground` |
| Install service | `sudo ./build/linux-x86_64-release/axiomdlp-agent --install` |
| Uninstall | `sudo ./build/linux-x86_64-release/axiomdlp-agent --uninstall` |
| View logs | `journalctl -u axiomdlp-agent -f` |
| Clean build | `rm -rf build && ./scripts/build-all.sh` |

### 12.5 Command-Line Arguments

| Argument | Effect |
|----------|--------|
| `--foreground` | Run without registering with the service manager |
| `--install` | Install as a service (unit file / SCM entry) |
| `--uninstall` | Remove the service |
| `--config <path>` | Use a specific config file |

Unknown arguments are logged to `stderr` but do not prevent startup (fail-safe).

---

## 13. Roadmap

### Milestone 5 — Module System

The plugin architecture. Every future DLP feature will be a module implementing a common interface:

- `IModule` interface (name, version, initialize, start, stop, shutdown, health)
- `ModuleManager` (drives lifecycle, reports aggregate health)
- `ServiceContext` (shared dependencies passed to modules)
- A dummy module to prove the pattern

### Milestone 6 — Event Bus

In-process publish/subscribe. Modules publish events; subscribers consume them. Decouples capture from scanning, deciding, and enforcing.

### Milestone 7 — Filesystem Module

First real DLP feature:

- Linux: `inotify` / `fanotify`
- Windows: `ReadDirectoryChangesW` / Minifilter

### Milestone 8 — Scanner

Content scanning:

- Regex patterns for credit cards, SSNs, emails, phone numbers
- Optional embedded ML for names, addresses

### Milestone 9 — Policy Engine

Rule evaluation:

- Policy → Decision mapping
- Priority-based matching
- Action selection (block/alert/log)

### Milestone 10 — Storage

Local persistence:

- SQLite for events, policies, and offline queue

### Milestone 11 — User-Mode Enforcement

Blocking operations:

- Clipboard paste block
- File write block
- USB write block

### Milestone 12 — IPC + User Helper

Tray icon + notifications:

- Linux: Unix domain sockets
- Windows: Named pipes

### Milestone 13 — Transport

Connection to AxiomDLP Core:

- HTTP + WebSocket
- Enrollment, heartbeat, event upload
- Policy sync

### Milestone 14 — Kernel Enforcement

Deeper blocking:

- Windows: Minifilter driver
- Linux: eBPF LSM / fanotify permission events

---

## 14. License

Apache License 2.0 — see the `LICENSE` file for details.

---

## Summary

The AxiomDLP Endpoint Agent currently provides:

- **Cross-platform build system** — four targets from one Linux host
- **Async structured logging** — spdlog with rotation, thread-safe, non-blocking
- **Fail-safe configuration** — JSON loading with defaults and warnings
- **Service lifecycle** — systemd (Linux) and SCM (Windows) with full install/uninstall
- **Platform abstraction** — one interface, two implementations, zero `#ifdef` in core code
- **Graceful shutdown** — signal-based (Linux), control-code based (Windows)
- **Safe subprocess spawning** — `fork` + `execvp`, no shell, no injection

**Both platforms are verified working.** The foundation is complete and production-quality. The next milestone begins DLP functionality.

**Quick start:**

```bash
git clone <repo>
cd axiomdlp-agent
./scripts/build-all.sh
sudo ./build/linux-x86_64-release/axiomdlp-agent --install
```