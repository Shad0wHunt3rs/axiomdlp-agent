# AxiomDLP Endpoint Agent — README

A cross-platform Data Leak Prevention (DLP) endpoint agent written in modern C++. Runs as a background service on Linux and Windows. Currently builds for four targets from a single Linux host.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Current Status](#2-current-status)
3. [Architecture](#3-architecture)
4. [Directory Structure](#4-directory-structure)
5. [Build System](#5-build-system)
6. [Core Subsystems](#6-core-subsystems)
7. [Platform Abstraction](#7-platform-abstraction)
8. [Building the Agent](#8-building-the-agent)
9. [Running the Agent](#9-running-the-agent)
10. [Installing as a Service](#10-installing-as-a-service)
11. [Configuration Reference](#11-configuration-reference)
12. [Dependencies](#12-dependencies)
13. [Development Workflow](#13-development-workflow)
14. [Roadmap](#14-roadmap)
15. [License](#15-license)

---

## 1. Project Overview

**AxiomDLP Endpoint Agent** is the endpoint-side component of the AxiomDLP platform. It runs as a privileged background service on user machines and is designed to:

- Monitor local activity (file access, clipboard, USB, printing, browser uploads)
- Detect sensitive data in real time
- Enforce DLP policies locally
- Work fully offline with a local policy cache and event queue
- Report events to the AxiomDLP Core backend
- Complement the existing email milter (which handles SMTP egress)

**Design goals:**

- **Cross-platform** — Linux and Windows from one codebase
- **Modular** — every feature is a plugin; new ones add without touching the core
- **Offline-first** — local policy cache and event queue
- **Kernel-ready** — enforcement abstraction allows user-mode → kernel-mode upgrades
- **Fail-safe** — bad config, missing libraries, or unavailable features never crash the agent
- **Observable** — structured async logging, health, config-driven behavior

**Status:** Foundation complete. The agent is a working service with logging, config, and lifecycle. No DLP features yet.

---

## 2. Current Status

### What Works Today

| Capability | Status |
|-----------|--------|
| Build for Linux x86_64 | ✅ Working |
| Build for Linux i686 | ✅ Working |
| Build for Windows x86_64 (cross-compiled from Linux) | ✅ Working |
| Build for Windows i686 (cross-compiled from Linux) | ✅ Working |
| Async structured logging (console + rotating file) | ✅ Working |
| Config loading from JSON with validation | ✅ Working |
| systemd service integration (Linux) | ✅ Working |
| Windows SCM service integration | ✅ Compiled (not yet tested on a live Windows machine) |
| Graceful shutdown via signals / SCM controls | ✅ Working |
| Auto-install as a service | ✅ Working |

### What Does Not Exist Yet

| Feature | Milestone |
|---------|-----------|
| Module system (plugin architecture) | 5 |
| Event bus | 6 |
| Filesystem monitoring | 7 |
| Local regex scanner | 8 |
| Policy engine | 9 |
| SQLite storage | 10 |
| User-mode enforcement | 11 |
| IPC + tray helper | 12 |
| Transport to core (HTTP + WSS) | 13 |
| Kernel enforcement (minifilter / eBPF) | 14 |

The current agent is an empty vessel: it starts, logs, reads config, and installs as a service. The next milestones fill it with DLP functionality.

---

## 3. Architecture

```
┌──────────────────────────────────────────────────────────┐
│                        main.cpp                          │
│                  Thin entry point (5 lines)              │
└──────────────────────────┬───────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────┐
│                     Application                          │
│  - Owns Config                                           │
│  - Owns Logger                                           │
│  - Owns IService (platform-specific)                     │
│  - Parses args, dispatches install/uninstall/run         │
└──────────────────────────┬───────────────────────────────┘
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
                    │   (systemd)       │      │  (SCM)            │
                    └───────────────────┘      └───────────────────┘
```

**Key design principles:**

1. **Interface in `core/`, implementation in `platform/<os>/`** — business logic never includes OS headers.
2. **CMake selects one platform file per target** — no runtime dispatch, no `#ifdef` in application code.
3. **`#ifdef HAVE_X` guards optional features** — same source file compiles for every target with different features enabled.
4. **Fail-safe configuration** — missing or invalid config produces warnings, not crashes.

---

## 4. Directory Structure

```
axiomdlp-agent/
├── CMakeLists.txt                     Build recipe
├── CMakePresets.json                  Named build configurations (8 total)
├── .gitignore
├── LICENSE
├── README.md
│
├── cmake/
│   └── toolchains/                    Cross-compile toolchain files
│       ├── linux-x86_64.cmake
│       ├── linux-i686.cmake
│       ├── windows-x86_64.cmake
│       └── windows-i686.cmake
│
├── config/
│   ├── agent.default.json             Default runtime config
│   └── agent.schema.json              JSON Schema for editors and CI
│
├── docs/                              Architecture and design documents
│
├── packaging/
│   ├── linux/systemd/                 Reference systemd unit file
│   └── windows/                       Reference PowerShell install script
│
├── scripts/
│   └── build-all.sh                   Builds all four targets in one command
│
└── src/
    ├── main.cpp                       Entry point
    │
    ├── core/                          Framework (no OS-specific code)
    │   ├── application.{h,cpp}        Orchestrates everything
    │   ├── config.{h,cpp}             JSON config loading + validation
    │   ├── logger.{h,cpp}             Async logging wrapper around spdlog
    │   ├── paths.h                    Platform path API
    │   └── service.h                  IService interface
    │
    └── platform/                      OS-specific implementations
        ├── linux/
        │   ├── paths_linux.cpp        /etc, /var/log, /var/lib
        │   └── service_linux.cpp      systemd + sd_notify
        └── windows/
            ├── paths_windows.cpp      C:\ProgramData\...
            └── service_win.cpp        SCM + ServiceMain
```

---

## 5. Build System

### 5.1 The Four Targets

From one Linux host, we build:

| Preset | Output | Purpose |
|--------|--------|---------|
| `linux-x86_64-{debug,release}` | `build/linux-x86_64-*/axiomdlp-agent` | Modern Linux servers |
| `linux-i686-{debug,release}` | `build/linux-i686-*/axiomdlp-agent` | Legacy 32-bit Linux |
| `windows-x86_64-{debug,release}` | `build/windows-x86_64-*/axiomdlp-agent.exe` | Modern Windows |
| `windows-i686-{debug,release}` | `build/windows-i686-*/axiomdlp-agent.exe` | Legacy 32-bit Windows |

### 5.2 Toolchain Files

Each target has a toolchain file in `cmake/toolchains/`. A toolchain tells CMake which compilers to use and where libraries live.

**Linux toolchains** use the host's native `gcc` / `g++`. The 32-bit Linux toolchain adds `-m32` flags.

**Windows toolchains** use MinGW-w64 cross-compilers (`x86_64-w64-mingw32-g++` and `i686-w64-mingw32-g++`). Key settings:

```cmake
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # Host tools
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # MinGW libraries only
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # MinGW headers only
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)    # MinGW CMake packages only
```

These four lines are what keep cross-compilation correct. They prevent Windows builds from accidentally pulling in Linux headers or libraries.

### 5.3 CMake Presets

`CMakePresets.json` defines eight configure presets and eight build presets. Instead of remembering long commands, you run:

```bash
cmake --preset linux-x86_64-release
cmake --build --preset linux-x86_64-release
```

List all available presets:

```bash
cmake --list-presets
```

### 5.4 Dependency Caching

Third-party sources are cached in `.deps/` so deletions of `build/` don't trigger re-downloads.

| Dependency | Source | Reason |
|------------|--------|--------|
| **spdlog** | FetchContent + `.deps/spdlog-src/` | Compiled library — must be built per target |
| **nlohmann_json** | System (`apt`) for Linux; `.deps/nlohmann_json-src/` for Windows | Header-only, but cross-compile needs isolated headers |

The `.deps/` directory is populated once. Every rebuild reuses it.

---

## 6. Core Subsystems

### 6.1 Logging (`src/core/logger.{h,cpp}`)

Wraps **spdlog** with two sinks:

- **Console sink** — colored output for terminals
- **Rotating file sink** — 5 MiB per file, 3 backups (~20 MiB cap)

Runs in **async mode**: the caller never blocks. A single background thread drains the queue and writes to both sinks.

**Key settings:**

| Setting | Value | Why |
|---------|-------|-----|
| Queue size | 8192 | Absorbs bursts without memory pressure |
| Overflow policy | `overrun_oldest` | Never block the agent; drop old logs under load |
| Pattern | `[time] [level] [thread] message` | Millisecond precision for concurrency debugging |
| Flush policy | `warn` and above | Important events survive crashes |

**Usage:**

```cpp
#include <spdlog/spdlog.h>
SPDLOG_INFO("event: {}", value);
SPDLOG_WARN("something odd");
SPDLOG_ERROR("something failed");
```

### 6.2 Configuration (`src/core/config.{h,cpp}`)

Reads a JSON config, validates values, applies defaults, returns a `Config` object. **Never throws, never crashes** — problems become warnings.

**Current sections:**

```
config/agent.default.json:
{
  "logging": {
    "dir":   "logs",
    "file":  "agent.log",
    "level": "info"
  }
}
```

**Design:**

- Nested structs mirror JSON structure.
- Every field has a built-in default.
- Loaded warnings are collected and replayed after logging is initialized.

See [Configuration Reference](#11-configuration-reference) for the full schema.

### 6.3 Service Lifecycle (`src/core/service.h`, `platform/*/service_*.cpp`)

`IService` is the platform-agnostic interface:

```cpp
class IService {
    virtual int  run() = 0;
    virtual void request_stop() = 0;
};
```

**Linux implementation** (`service_linux.cpp`):

- Installs signal handlers for `SIGTERM` / `SIGINT`
- Ignores `SIGHUP`
- Calls `sd_notify(READY=1)` when started (only if `HAVE_SYSTEMD`)
- Main loop with `std::atomic<bool>` stop flag
- `install_service()` writes a systemd unit file

**Windows implementation** (`service_win.cpp`):

- Connects to Service Control Manager via `StartServiceCtrlDispatcherA`
- `service_main` callback registers a control handler
- Reports status via `SetServiceStatus`
- `install_service()` creates an SCM entry via `CreateServiceA`

Both implementations share the same stop-flag pattern: a global `std::atomic<bool>` set by signal/SCM handler, checked by the main loop every 200 ms.

### 6.4 Paths (`src/core/paths.h`, `platform/*/paths_*.cpp`)

Five platform-specific functions:

| Function | Linux | Windows |
|----------|-------|---------|
| `executable_path()` | `readlink("/proc/self/exe")` | `GetModuleFileNameA` |
| `executable_dir()` | `path.parent_path()` | `path.parent_path()` |
| `default_config_path()` | `/etc/axiomdlp/agent.json` | `C:\ProgramData\AxiomDLP\agent.json` |
| `default_log_dir()` | `/var/log/axiomdlp` | `C:\ProgramData\AxiomDLP\logs` |
| `default_data_dir()` | `/var/lib/axiomdlp` | `C:\ProgramData\AxiomDLP` |

Follows the Filesystem Hierarchy Standard on Linux, and Windows conventions on Windows.

### 6.5 Application (`src/core/application.{h,cpp}`)

Owns the lifecycle. Reads args, loads config, initializes logging, then either:

- **installs the service** (`--install`)
- **uninstalls the service** (`--uninstall`)
- **runs as a service** (default)

Shutdown is automatic via destructor, ensuring logs are always flushed.

---

## 7. Platform Abstraction

The core principle that makes this project cross-platform:

```
┌──────────────────────────────────────────────────────────┐
│                     core/                                │
│  No OS headers. No #ifdef _WIN32. Only interfaces.       │
└──────────────────────────┬───────────────────────────────┘
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
    ┌──────────────────┐      ┌──────────────────┐
    │  platform/linux/ │      │ platform/windows/ │
    │  Real Linux code │      │ Real Windows code │
    └──────────────────┘      └──────────────────┘
              ▲                         ▲
              │                         │
              └──── CMake picks one ────┘
```

**How it works:**

1. `core/` declares what operations are needed (`IService`, `paths.h`).
2. `platform/<os>/` provides implementations.
3. CMake's `if(WIN32)` selects one set of platform files per target.
4. The linker sees exactly one definition of each function.

**Optional features** (like systemd's `sd_notify`) use a second layer of CMake detection:

```cmake
if(PkgConfig_FOUND)
    pkg_check_modules(SYSTEMD QUIET libsystemd)
    if(SYSTEMD_FOUND)
        target_compile_definitions(axiomdlp-agent PRIVATE HAVE_SYSTEMD)
    endif()
endif()
```

The C++ code then guards the feature:

```cpp
#ifdef HAVE_SYSTEMD
    sd_notify(0, "READY=1");
#endif
```

**Result:** One source file compiles for every target, with the correct feature set for each.

---

## 8. Building the Agent

### 8.1 One-Time Setup

Install the required toolchain:

```bash
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

Enable the i386 architecture first (for 32-bit libsystemd):

```bash
sudo dpkg --add-architecture i386
sudo apt update
```

Populate the nlohmann_json source cache (one-time):

```bash
mkdir -p ~/axiomdlp-agent/.deps
cd ~/axiomdlp-agent/.deps
git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git nlohmann_json-src
```

*Note: the `scripts/build-all.sh` script performs this automatically on first run.*

### 8.2 Build All Targets

```bash
cd ~/axiomdlp-agent
./scripts/build-all.sh
```

This builds all four targets and prints their formats. First run: ~2 minutes (includes dependency clone). Subsequent runs: ~1 minute.

### 8.3 Build a Single Target

```bash
# Configure once
cmake --preset linux-x86_64-release

# Build
cmake --build --preset linux-x86_64-release

# Debug build instead
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug
```

### 8.4 Clean Rebuild

```bash
rm -rf build
./scripts/build-all.sh
```

The `.deps/` cache survives this — no re-downloads.

---

## 9. Running the Agent

### 9.1 Foreground Mode (Development)

Run from the project root so the default config path resolves:

```bash
cd ~/axiomdlp-agent
./build/linux-x86_64-release/axiomdlp-agent --foreground
```

Expected output:

```
[2026-09-19 12:00:00.000] [info] [t:12345] AxiomDLP Agent v0.1.0
[2026-09-19 12:00:00.000] [info] [t:12345] Config loaded from: config/agent.default.json
[2026-09-19 12:00:00.000] [info] [t:12345] axiomdlp-agent starting (Linux service mode)
[2026-09-19 12:00:00.000] [info] [t:12345] sd_notify READY sent
```

Press `Ctrl+C` to stop. The agent shuts down gracefully:

```
[info] axiomdlp-agent received stop request
[info] Service returned rc=0
[info] Application shutting down
```

### 9.2 Log Files

Logs are written to two places:

- **Console** (colored) — during foreground runs
- **File** — `logs/agent.log` (relative to working directory)

The file rotates at 5 MiB and keeps 3 backups (`agent.1.log`, `agent.2.log`, `agent.3.log`).

---

## 10. Installing as a Service

### 10.1 Linux (systemd)

Install the service:

```bash
sudo ./build/linux-x86_64-release/axiomdlp-agent --install
```

This writes `/etc/systemd/system/axiomdlp-agent.service`. Output tells you the next steps:

```
[info] installed unit file: /etc/systemd/system/axiomdlp-agent.service
[info] next: systemctl daemon-reload && systemctl enable --now axiomdlp-agent
```

Run them:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now axiomdlp-agent
sudo systemctl status axiomdlp-agent
```

Expected status:

```
● axiomdlp-agent.service - AxiomDLP Endpoint Agent
     Loaded: loaded (/etc/systemd/system/axiomdlp-agent.service; enabled)
     Active: active (running) since ...
```

**View logs:**

```bash
journalctl -u axiomdlp-agent -f
```

**Stop / start / disable:**

```bash
sudo systemctl stop axiomdlp-agent
sudo systemctl start axiomdlp-agent
sudo systemctl disable axiomdlp-agent
```

**Uninstall:**

```bash
sudo systemctl disable --now axiomdlp-agent
sudo ./build/linux-x86_64-release/axiomdlp-agent --uninstall
sudo systemctl daemon-reload
```

### 10.2 Windows (SCM)

From an elevated (Administrator) Command Prompt:

```cmd
axiomdlp-agent.exe --install
```

This creates the service via `CreateService`. Then:

```cmd
sc start AxiomDLPAgent
sc query AxiomDLPAgent
```

**Uninstall:**

```cmd
sc stop AxiomDLPAgent
axiomdlp-agent.exe --uninstall
```

The Windows service implementation compiles and cross-builds correctly. Full runtime testing on a Windows machine is a pending milestone item.

---

## 11. Configuration Reference

### 11.1 File Location

The agent loads `config/agent.default.json` relative to the current working directory. Default locations for production (implemented in later milestones) will be:

- **Linux:** `/etc/axiomdlp/agent.json`
- **Windows:** `C:\ProgramData\AxiomDLP\agent.json`

### 11.2 Current Schema

```json
{
  "logging": {
    "dir":   "logs",
    "file":  "agent.log",
    "level": "info"
  }
}
```

### 11.3 Field Reference

| Field | Type | Default | Allowed Values |
|-------|------|---------|----------------|
| `logging.dir` | string | `"logs"` | Any directory path |
| `logging.file` | string | `"agent.log"` | Any filename |
| `logging.level` | string | `"info"` | `trace`, `debug`, `info`, `warn`, `error`, `critical`, `off` |

### 11.4 Behavior

- **Missing file** → log warning, use defaults.
- **Invalid JSON** → log warning, use defaults.
- **Field wrong type** → log warning, use default for that field.
- **`level` invalid** → log warning, use `"info"`.
- **Missing field** → silently use default.

The agent **always starts**, regardless of config problems.

### 11.5 Extending the Config

Adding a new section requires:

1. A struct in `src/core/config.h`
2. A field in `Config`
3. A block in `src/core/config.cpp`'s `load()`
4. A section in `config/agent.default.json`
5. An entry in `config/agent.schema.json`

Existing config files remain valid — missing sections get defaults.

---

## 12. Dependencies

### 12.1 Runtime Dependencies

| Library | Purpose | How It's Used |
|---------|---------|---------------|
| **spdlog** | Async structured logging | FetchContent with `.deps/spdlog-src/` cache |
| **nlohmann/json** | JSON parsing | System `apt` on Linux; `.deps/nlohmann_json-src/` on Windows |
| **libsystemd** | `sd_notify` | Detected via pkg-config (optional) |
| **libfmt** | String formatting | Transitive dependency of spdlog |

### 12.2 Build Dependencies

| Tool | Purpose |
|------|---------|
| **CMake** ≥ 3.24 | Build system |
| **Ninja** | Build generator |
| **GCC 13+** or **Clang 17+** | Linux compilation |
| **MinGW-w64** | Windows cross-compilation |
| **pkg-config** | Library detection |

### 12.3 Windows Runtime Requirements

Windows binaries are **fully static** — no MSVC redistributable, no `libstdc++-6.dll`, no `libgcc_s_seh-1.dll`. The `.exe` runs on any Windows 7 or later system with no installation.

---

## 13. Development Workflow

### 13.1 Typical Iteration

```bash
# Edit code
$EDITOR src/core/logger.cpp

# Build one target (fast)
cmake --build --preset linux-x86_64-debug

# Run
./build/linux-x86_64-debug/axiomdlp-agent --foreground

# Iterate...
```

### 13.2 Full Cross-Platform Check

Before committing, verify all four targets build:

```bash
./scripts/build-all.sh
```

### 13.3 Editor Setup (VS Code)

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

Switch between configs from the status bar (bottom-right).

### 13.4 Common Commands

| Task | Command |
|------|---------|
| Build all targets | `./scripts/build-all.sh` |
| Build one target | `cmake --build --preset linux-x86_64-release` |
| Run in foreground | `./build/linux-x86_64-release/axiomdlp-agent --foreground` |
| Install as service | `sudo ./build/linux-x86_64-release/axiomdlp-agent --install` |
| Uninstall | `sudo ./build/linux-x86_64-release/axiomdlp-agent --uninstall` |
| View systemd logs | `journalctl -u axiomdlp-agent -f` |
| View file logs | `tail -f logs/agent.log` |
| Clean build | `rm -rf build && ./scripts/build-all.sh` |

### 13.5 Command-Line Arguments

| Argument | Effect |
|----------|--------|
| `--foreground` | Run without registering with systemd/SCM |
| `--install` | Install the service (unit file / SCM entry) |
| `--uninstall` | Remove the service |
| `--config <path>` | Use a specific config file |

---

## 14. Roadmap

The foundation is complete. The following milestones add DLP functionality.

### Completed

- ✅ **Milestone 1** — Project skeleton
- ✅ **Milestone 2** — Async logging with spdlog
- ✅ **Milestone 3** — Configuration loading with JSON
- ✅ **Milestone 4** — Cross-platform service lifecycle

### Planned

| # | Milestone | Delivers |
|---|-----------|----------|
| 5 | **Application class refactor** | Consolidate startup, prepare for modules |
| 6 | **Module system** | Plugin architecture — every feature is a module |
| 7 | **Event bus** | In-process pub/sub for decoupled modules |
| 8 | **Filesystem module** | inotify (Linux) / ReadDirectoryChangesW (Windows) |
| 9 | **Scanner** | Regex detection of PII patterns |
| 10 | **Policy engine** | Event + scan → decision |
| 11 | **Storage** | SQLite for events, policies, and offline queue |
| 12 | **User-mode enforcement** | Block clipboard / file operations |
| 13 | **IPC + tray helper** | User session UI |
| 14 | **Transport** | HTTP + WebSocket to AxiomDLP Core |
| 15 | **Windows testing** | Runtime verification on live Windows |
| 16 | **Kernel enforcement** | Windows Minifilter / Linux eBPF LSM |

### Design Guarantees

Every future milestone will:

- Follow the interface + platform implementation pattern
- Use the same three-file structure (header, `.cpp`, CMake)
- Compile for all four targets
- Have no dependencies on optional system features

---

## 15. License

Apache License 2.0 — see the `LICENSE` file for details.

---

## Summary

The AxiomDLP Endpoint Agent currently provides:

- **Cross-platform build system** — four targets from one Linux host, no downloads after first run
- **Async structured logging** — never blocks, rotates, survives crashes
- **Fail-safe configuration** — JSON with validation, defaults, and warnings
- **Service lifecycle** — systemd and Windows SCM integration with graceful shutdown
- **Platform abstraction** — one interface, two implementations, no `#ifdef` in core code

The foundation is complete and production-quality. The next milestones add the DLP functionality that justifies the agent's existence.

**To get started:**

```bash
git clone <repo>
cd axiomdlp-agent
./scripts/build-all.sh
./build/linux-x86_64-release/axiomdlp-agent --foreground
```