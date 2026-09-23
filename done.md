# AxiomDLP Endpoint Agent — README

A cross-platform Data Leak Prevention (DLP) endpoint agent written in modern C++20. Runs as a native background service on Linux and Windows. Builds four targets from a single Linux host.

**Current status:** Foundation and plugin architecture complete. No DLP features yet — the agent is a working service that hosts modules but does no monitoring, scanning, or enforcement.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Current Status](#2-current-status)
3. [Architecture](#3-architecture)
4. [Directory Structure](#4-directory-structure)
5. [Build System](#5-build-system)
6. [Dependencies](#6-dependencies)
7. [Configuration](#7-configuration)
8. [Running as a Service](#8-running-as-a-service)
9. [Platform Abstraction](#9-platform-abstraction)
10. [Module System](#10-module-system)
11. [Development Workflow](#11-development-workflow)
12. [Roadmap](#12-roadmap)
13. [Known Limitations](#13-known-limitations)
14. [License](#14-license)

---

## 1. Overview

**AxiomDLP Endpoint Agent** is the endpoint-side component of the AxiomDLP platform. It runs as a privileged background service on user machines and is designed to:

- Monitor local activity (filesystem, USB, clipboard, printing, browser)
- Detect sensitive data in real time
- Enforce DLP policies locally
- Work fully offline with a local cache
- Report events to the AxiomDLP Core backend
- Complement the existing email milter (which handles SMTP egress)

**Design goals:**

- **Cross-platform** — Linux and Windows from one codebase
- **Modular** — every feature is a plugin; new ones add without touching the core
- **Offline-first** — local policy cache and event queue (future)
- **Kernel-ready** — enforcement abstraction allows user-mode → kernel-mode upgrades (future)
- **Fail-safe** — bad config, missing libraries, or unavailable features never crash the agent
- **Observable** — structured async logging, health reporting, config-driven behavior

**Status:** Foundations complete. The agent runs as a service, reads config, logs, and hosts modules. Real DLP work begins in Milestone 6.

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
| systemd service integration (Linux) | ✅ Verified |
| Windows SCM service integration | ✅ Verified |
| Graceful shutdown via signals / SCM controls | ✅ Verified |
| One-command install / uninstall | ✅ Working |
| Module system (plugin architecture) | ✅ Working |
| Cross-platform VS Code IntelliSense via clangd | ✅ Working |

### What Does Not Exist Yet

| Feature | Milestone |
|---------|-----------|
| Event bus (pub/sub between modules) | 6 |
| Filesystem monitoring | 7 |
| Local regex scanner | 8 |
| Policy engine | 9 |
| SQLite storage | 10 |
| User-mode enforcement | 11 |
| IPC + tray helper | 12 |
| Transport to core (HTTP + WebSocket) | 13 |
| Kernel enforcement (minifilter / eBPF) | 14 |

The current agent is a **working vessel with no cargo**. It starts, logs, reads config, hosts modules, and stops cleanly. The next milestones fill it with DLP functionality.

---

## 3. Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        main.cpp                              │
│                   Thin entry point (5 lines)                 │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                      Application                             │
│  - Loads Config                                              │
│  - Initializes Logger                                        │
│  - Creates ServiceContext                                    │
│  - Creates ModuleManager                                     │
│  - Registers modules                                         │
│  - Dispatches install / uninstall / run                      │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                      Service                                 │
│  - Owns the module lifecycle                                 │
│  - Installs signal / SCM handlers                            │
│  - Starts modules                                            │
│  - Signals readiness (sd_notify / SetServiceStatus)          │
│  - Runs main loop                                            │
│  - Stops modules                                             │
│  - Signals stopping                                          │
└──────────────────────────┬───────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                   ModuleManager                              │
│  - Owns all modules                                          │
│  - Drives their lifecycle (init → start → stop → shutdown)   │
│  - Aggregates their health                                   │
└──────────────────────────┬───────────────────────────────────┘
                           │
              ┌────────────┴────────────┐
              ▼                         ▼
    ┌──────────────────┐      ┌──────────────────┐
    │  ExampleModule   │      │  (future modules)│
    │  (proof of       │      │  Filesystem, USB,│
    │   concept)       │      │  Scanner, ...    │
    └──────────────────┘      └──────────────────┘
```

**Key design principles:**

1. **Interface in `core/`, implementation in `platform/<os>/`** — business logic never includes OS headers.
2. **CMake selects one platform file per target** — no runtime dispatch, no `#ifdef` in application code.
3. **Modules are the extension point** — every future DLP feature is a module.
4. **Fail-safe** — missing config, failed modules, unavailable features never crash the agent.
5. **RAII** — destructors flush logging and release resources, even on error paths.

---

## 4. Directory Structure

```
axiomdlp-agent/
├── CMakeLists.txt                     Build recipe
├── CMakePresets.json                  8 named build configurations
├── .clangd                            clangd config (points at merged DB)
├── .vscode/
│   └── settings.json                  clangd + query-driver config
├── .gitignore
├── LICENSE
├── README.md
│
├── .deps/                             Cached third-party sources (gitignored)
│   ├── spdlog-src/
│   └── nlohmann_json-src/
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
│   └── agent.schema.json              JSON Schema for editors
│
├── docs/                              Architecture documents
│
├── packaging/
│   ├── linux/systemd/                 Reference systemd unit file
│   └── windows/                       Reference PowerShell install script
│
├── scripts/
│   ├── build-all.sh                   Build all four targets
│   └── merge-compile-commands.sh      Merge 6 DBs into one (for clangd)
│
└── src/
    ├── main.cpp                       Entry point (5 lines)
    │
    ├── core/                          Framework (no OS-specific code)
    │   ├── application.{h,cpp}        Orchestration
    │   ├── config.{h,cpp}             JSON config loading + validation
    │   ├── logger.{h,cpp}             Async logging wrapper
    │   ├── module_manager.{h,cpp}     Module lifecycle orchestrator
    │   ├── health.h                   Health status types
    │   ├── service_context.h          Shared dependencies container
    │   ├── paths.h                    Platform path API
    │   └── service.h                  IService interface
    │
    ├── interfaces/
    │   └── i_module.h                 Module interface (7 methods)
    │
    ├── modules/
    │   ├── module_base.{h,cpp}        Convenience base class for modules
    │   └── example/
    │       └── example_module.{h,cpp} Proof-of-concept module
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

From one Linux host, we build four binaries:

| Preset | Output | Purpose |
|--------|--------|---------|
| `linux-x86_64-{debug,release}` | `build/linux-x86_64-*/axiomdlp-agent` | Modern Linux |
| `linux-i686-{debug,release}` | `build/linux-i686-*/axiomdlp-agent` | Legacy 32-bit Linux |
| `windows-x86_64-{debug,release}` | `build/windows-x86_64-*/axiomdlp-agent.exe` | Modern Windows |
| `windows-i686-{debug,release}` | `build/windows-i686-*/axiomdlp-agent.exe` | Legacy 32-bit Windows |

### 5.2 Toolchain Files

Each target has a toolchain file in `cmake/toolchains/`.

**Linux toolchains** use the host's native `gcc` / `g++`. The 32-bit Linux toolchain adds `-m32`.

**Windows toolchains** use MinGW-w64 cross-compilers (`x86_64-w64-mingw32-g++` and `i686-w64-mingw32-g++`) with strict root path isolation:

```cmake
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # Host tools
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # MinGW libraries only
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # MinGW headers only
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)    # MinGW packages only
```

These four lines prevent cross-compilation contamination — a Linux `.so` can never link into a Windows `.exe`.

### 5.3 Dependency Caching

Third-party sources are cached in `.deps/` so deleting `build/` doesn't trigger re-downloads.

| Dependency | Source | Reason |
|------------|--------|--------|
| **spdlog** | FetchContent + `.deps/spdlog-src/` | Compiled library — must be built per target |
| **nlohmann_json** | System (`apt`) on Linux; `.deps/nlohmann_json-src/` on Windows | Header-only, but cross-compile needs isolated headers |

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

### 5.5 Building All Targets

```bash
cd ~/axiomdlp-agent
./scripts/build-all.sh
```

First run: ~2 minutes. Subsequent runs: ~1 minute.

### 5.6 Building a Single Target

```bash
cmake --preset linux-x86_64-debug
cmake --build --preset linux-x86_64-debug
```

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
| **clangd** | Editor IntelliSense (optional but recommended) |

### Windows Runtime Requirements

Windows binaries are **fully static** — no MSVC redistributable, no `libstdc++-6.dll`, no `libgcc_s_seh-1.dll`. The `.exe` runs on any Windows 7+ system.

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

`config/agent.schema.json` is a JSON Schema used by editors for autocomplete and by CI for validation.

---

## 8. Running as a Service

### 8.1 Linux (systemd)

**Install** (requires root):

```bash
sudo ./build/linux-x86_64-release/axiomdlp-agent --install
```

Full sequence:
1. Write `/etc/systemd/system/axiomdlp-agent.service`
2. `systemctl daemon-reload`
3. `systemctl enable --now axiomdlp-agent`

**Expected output:**

```
[info] installed unit file: /etc/systemd/system/axiomdlp-agent.service
[info] reloaded systemd configuration
[info] enabled and started service axiomdlp-agent
```

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

### 8.2 Windows (SCM)

**Install** (requires Administrator Command Prompt):

```cmd
axiomdlp-agent.exe --install
```

Full sequence:
1. `CreateServiceA` — register with SCM (auto-start)
2. `StartServiceA` — start immediately

**Verify:**

```cmd
sc query AxiomDLPAgent
```

Should show `STATE : 4 RUNNING`.

**Uninstall:**

```cmd
axiomdlp-agent.exe --uninstall
```

Full sequence:
1. `ControlService(STOP)` and wait for stop
2. `DeleteService`

### 8.3 Foreground Mode (Development)

Run in a terminal without registering as a service:

```bash
./build/linux-x86_64-release/axiomdlp-agent --foreground
```

Or on Windows:

```cmd
axiomdlp-agent.exe --foreground
```

Press `Ctrl+C` to stop. On Windows, Ctrl+C is not yet implemented — use Task Manager or close the terminal.

### 8.4 Automatic Mode Detection

The agent automatically detects how it was started:

| Platform | Terminal | Service |
|----------|----------|---------|
| **Linux** | Same code path — signals work | Same code path — signals from systemd |
| **Windows** | `StartServiceCtrlDispatcherA` fails → foreground fallback | SCM drives the service lifecycle |

**Neither platform requires the `--foreground` flag** — it's parsed but behavior is automatic.

---

## 9. Platform Abstraction

The core principle that makes the project cross-platform:

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

### 9.1 Interfaces in `core/`

| Interface | Purpose |
|-----------|---------|
| `service.h` | `IService` — service lifecycle |
| `paths.h` | Path helpers — executable location, config, log, data directories |

### 9.2 Implementations in `platform/<os>/`

| File | Purpose |
|------|---------|
| `platform/linux/service_linux.cpp` | systemd + signal handling + `sd_notify` |
| `platform/linux/paths_linux.cpp` | `/proc/self/exe`, `/etc`, `/var/log`, `/var/lib` |
| `platform/windows/service_win.cpp` | SCM + `ServiceMain` + control handler |
| `platform/windows/paths_windows.cpp` | `GetModuleFileNameA`, `SHGetKnownFolderPath` |

### 9.3 Optional Features

Optional features use build-time detection:

```cmake
if(PkgConfig_FOUND)
    pkg_check_modules(SYSTEMD QUIET libsystemd)
    if(SYSTEMD_FOUND)
        target_compile_definitions(axiomdlp-agent PRIVATE HAVE_SYSTEMD)
    endif()
endif()
```

Then in C++:

```cpp
#ifdef HAVE_SYSTEMD
    sd_notify(0, "READY=1");
#endif
```

Same source file compiles for every target with the correct feature set.

---

## 10. Module System

The plugin architecture that every future DLP feature will use.

### 10.1 What Is a Module

A **module** is a self-contained feature. Every module:

- Owns its own threads and state
- Has a uniform interface
- Is managed by the `ModuleManager`
- Never calls other modules directly

### 10.2 The Interface — `IModule`

Seven methods every module implements:

| Method | Purpose |
|--------|---------|
| `name()` | Module identifier (e.g., `"filesystem"`) |
| `version()` | Semantic version string |
| `initialize(ctx)` | One-time setup with shared context |
| `start()` | Begin work |
| `stop()` | Stop work (idempotent) |
| `shutdown()` | Release resources |
| `health()` | Report current status |

### 10.3 The Manager — `ModuleManager`

Responsibilities:

| Method | When called |
|--------|-------------|
| `register_module()` | At startup, by the Application |
| `initialize_all(ctx)` | Once, after all modules registered |
| `start_all()` | Inside `Service::run()`, before signaling ready |
| `stop_all()` | Inside `Service::run()`, on shutdown (reverse order) |
| `shutdown_all()` | After all stopped (reverse order) |
| `health()` | On demand |

**Fail-safe:** If one module fails to initialize or start, the others still run. The agent never goes down because of one broken module.

**Ordering:** Stop and shutdown happen in **reverse registration order** — last registered stops first. This ensures dependent modules stop before their dependencies.

### 10.4 The Base Class — `ModuleBase`

A convenience base class that provides default implementations for the trivial methods. Modules inherit from `ModuleBase` and override only what they need.

```cpp
class ExampleModule : public ModuleBase {
    std::string name() const override { return "example"; }
    bool start() override { /* ... */ return true; }
    void stop() override { /* ... */ }
};
```

**Six of the seven methods come from the base class.** Only `name()` must be overridden.

### 10.5 The Shared Context — `ServiceContext`

Holds references to all subsystems that modules might need. Currently just `config`. Future milestones add:

- Event bus
- Storage
- Policy engine
- Transport

Modules receive the entire context. Adding a new subsystem doesn't change any module's signature.

### 10.6 Health Reporting

Every module reports a `HealthStatus`:

| Level | Meaning |
|-------|---------|
| OK | Fully working |
| Degraded | Working with issues |
| Failed | Not working |
| Unknown | Not yet checked |

The manager aggregates all module health into one summary.

### 10.7 Adding a New Module

The three-step process:

1. **Create the module class** — inherit from `ModuleBase`, override `name()` and any lifecycle methods.
2. **Register it** — one line in `Application::init()`.
3. **Compile** — done. The service loop never changes.

---

## 11. Development Workflow

### 11.1 Typical Iteration

```bash
# Edit code
$EDITOR src/core/logger.cpp

# Build one target (fast)
cmake --build --preset linux-x86_64-debug

# Run in foreground
./build/linux-x86_64-debug/axiomdlp-agent --foreground
```

### 11.2 Full Cross-Platform Check

```bash
./scripts/build-all.sh
```

### 11.3 Editor Setup (VS Code + clangd)

The project uses **clangd** for IntelliSense. This is required because the Microsoft C/C++ extension cannot handle cross-compilation (it uses one compiler for the whole workspace).

**Setup:**

1. Install clangd:
   ```bash
   sudo apt install clangd
   ```

2. Install the clangd extension in VS Code (`llvm-vs-code-extensions.vscode-clangd`).

3. Disable the Microsoft C/C++ extension (workspace-level).

4. The project ships:
   - `.clangd` — points clangd at the merged compile database
   - `.vscode/settings.json` — configures clangd with `--query-driver`

5. Regenerate the merged compile database after each build:
   ```bash
   ./scripts/merge-compile-commands.sh
   ```

6. Restart clangd in VS Code: `Ctrl+Shift+P` → **clangd: Restart language server**.

**Result:** Both Linux files (`readlink`, `<unistd.h>`) and Windows files (`<windows.h>`, `SHGetKnownFolderPath`) get correct IntelliSense simultaneously. No switching, no config changes.

### 11.4 Common Commands

| Task | Command |
|------|---------|
| Build all targets | `./scripts/build-all.sh` |
| Build one target | `cmake --build --preset linux-x86_64-release` |
| Run in foreground | `./build/linux-x86_64-release/axiomdlp-agent --foreground` |
| Install service | `sudo ./build/linux-x86_64-release/axiomdlp-agent --install` |
| Uninstall | `sudo ./build/linux-x86_64-release/axiomdlp-agent --uninstall` |
| View logs (systemd) | `journalctl -u axiomdlp-agent -f` |
| View logs (file) | `tail -f logs/agent.log` |
| Merge compile DBs | `./scripts/merge-compile-commands.sh` |
| Clean build | `rm -rf build && ./scripts/build-all.sh` |

### 11.5 Command-Line Arguments

| Argument | Effect |
|----------|--------|
| `--foreground` | Run without registering with the service manager |
| `--install` | Install as a service |
| `--uninstall` | Remove the service |
| `--config <path>` | Use a specific config file |

Unknown arguments are logged to `stderr` but do not prevent startup.

---

## 12. Roadmap

### Completed

- ✅ **Milestone 1** — Project skeleton
- ✅ **Milestone 2** — Async logging with spdlog
- ✅ **Milestone 3** — Configuration loading
- ✅ **Milestone 4** — Cross-platform service lifecycle
- ✅ **Milestone 5** — Module system

### Planned

| # | Milestone | Delivers |
|---|-----------|----------|
| 6 | **Event Bus** | Pub/sub between modules |
| 7 | **Filesystem Module** | First real event source |
| 8 | **Scanner** | Regex detection of PII patterns |
| 9 | **Policy Engine** | Event + scan → decision |
| 10 | **Storage** | SQLite for events, policies, offline queue |
| 11 | **User-Mode Enforcement** | Block clipboard / file operations |
| 12 | **IPC + Tray Helper** | User session UI |
| 13 | **Transport** | HTTP + WebSocket to AxiomDLP Core |
| 14 | **Kernel Enforcement** | Windows Minifilter / Linux eBPF LSM |
| 15 | **Packaging** | Signed MSI, .deb, .rpm |

---

## 13. Known Limitations

### 13.1 Config File Not Found When Running as Service

**Symptom:**

```
[warning] Config: config file not found at 'config/agent.default.json'
```

**Cause:** The service's working directory is `/` (Linux) or `C:\Windows\System32` (Windows). The relative path doesn't resolve.

**Impact:** The agent uses built-in defaults. Logging works, service works, but custom config isn't loaded.

**Planned fix:** Config path search (Milestone 6 or later).

### 13.2 No Ctrl+C Handler in Windows Foreground Mode

Pressing Ctrl+C in a Windows Command Prompt doesn't stop the agent cleanly. You must close the terminal window or use Task Manager.

**Cause:** Windows doesn't use POSIX signals. It uses console control handlers, which aren't registered.

**Impact:** Only affects foreground mode. Service mode stops cleanly via `SERVICE_CONTROL_STOP`.

**Planned fix:** Add `SetConsoleCtrlHandler` (later milestone).

### 13.3 service_context Only Has Config

The `ServiceContext` currently contains only the config. The event bus, storage, policy engine, and transport will be added in later milestones. Until then, modules can only access config.

---

## 14. License

Apache License 2.0 — see the `LICENSE` file for details.

---

## Summary

The AxiomDLP Endpoint Agent currently provides:

- **Cross-platform build system** — four targets from one Linux host
- **Async structured logging** — spdlog with rotation, thread-safe, non-blocking
- **Fail-safe configuration** — JSON loading with defaults and warnings
- **Service lifecycle** — systemd (Linux) and SCM (Windows) with one-command install/uninstall
- **Platform abstraction** — interfaces in `core/`, implementations in `platform/<os>/`
- **Module system** — plugin architecture where every future feature is a module
- **Module lifecycle** — driven by the service, not the Application
- **Health reporting** — every module reports status; the manager aggregates
- **Cross-platform IntelliSense** — clangd with merged compile database

**The foundation is complete and production-quality.** The next milestones add the DLP functionality.

**Quick start:**

```bash
git clone <repo>
cd axiomdlp-agent
./scripts/build-all.sh
./build/linux-x86_64-release/axiomdlp-agent --foreground
```y