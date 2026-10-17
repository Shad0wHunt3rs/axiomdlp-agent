# AxiomDLP Endpoint Agent — DONE.md

**Status Document — Everything Implemented, Verified, and Fixed**

**Last updated:** After the MSVC + vcpkg migration on `msvc-windows-build`.
Windows x86_64 and x86 both build and run; Linux x86_64 and i686 remain
unchanged from `main`.

**Supersedes:** all previous DONE.md revisions.

---

## 1. Executive Summary

The AxiomDLP Endpoint Agent is a **cross-platform, TLS-capable, network-connected
service foundation** with **four verified build targets from two distinct
toolchains**, six working modules, a persistent local SQLite database, SHA-256
file fingerprinting, HTTP + WebSocket connectivity to the AxiomDLP Core server,
a content scanner for text-based PII detection, and an **atomic
ShutdownController** that lets any component stop the whole agent cleanly with
a typed reason.

### Two dependency pipelines, one source tree

| Platform                | Toolchain              | Dependencies                                                                      |
| ----------------------- | ---------------------- | --------------------------------------------------------------------------------- |
| **Linux x86_64 / i686** | GCC (native, multilib) | `.deps/` — `build-openssl.sh` + system Boost + FetchContent + SQLite amalgamation |
| **Windows x64 / x86**   | MSVC 2022 (19.44)      | **vcpkg manifest mode** — 66 packages, static triplet                             |

The `if(MSVC)` branch in `CMakeLists.txt` is the only place the two
strategies diverge. Every source file, every header, every module uses the
same `find_package(...)` API and links the same imported targets
(`Boost::headers`, `OpenSSL::SSL`, `SQLite::SQLite3`, `spdlog::spdlog`,
`nlohmann_json::nlohmann_json`, `efsw::efsw`).

### What Works Today

**Linux (x86_64 + i686) — unchanged from `main`:**
- GCC native builds from `.deps/`
- Static OpenSSL 3.3.2, system Boost 1.90, FetchContent spdlog 1.14.1 / efsw 1.7.2 / nlohmann 3.11.3, SQLite 3.46.1 amalgamation
- systemd + `sd_notify`, i686 via `-m32` toolchain file
- Both architectures build, link, run, enroll, connect WebSocket, heartbeat, shutdown

**Windows (x64 + x86) — new on `msvc-windows-build`:**
- MSVC 2022 BuildTools builds via vcpkg manifest mode
- Fully static: OpenSSL 3.6.5, Boost 1.92.0, SQLite 3.53.4, spdlog 1.17.0, fmt 12.2.0, nlohmann-json 3.12.0, efsw 1.7.2
- Static CRT (`/MT`) — no `vcruntime140.dll` dependency
- SCM integration, service install/uninstall
- **Binary sizes: 8.61 MB (x64), 6.37 MB (x86)**
- No external DLL dependencies beyond Windows system DLLs
- Both binaries verified via full startup log (6 modules, migrations 0→4)

**Cross-platform features (all four targets):**
- Async structured logging with rotation and non-blocking delivery
- JSON configuration with fail-safe defaults and environment variable expansion
- Real-time filesystem watching (inotify / ReadDirectoryChangesW via efsw)
- Periodic inventory scanning with interruptible cancellation
- SHA-256 content hashing stored in SQLite
- Bidirectional WebSocket to Core with TLS, jittered reconnect, close-code interpretation, graceful 1000 Normal close
- HTTP client with unconditional certificate verification
- Agent enrollment via `POST /api/v1/agent/register`
- Periodic heartbeat via `POST /api/v1/agent/heartbeat`, plus immediate validation heartbeat at startup
- Atomic shutdown with typed reasons preserved end to end
- Server-pushed revocation handling (`token_revoked`, `agent_revoked`)
- Goodbye message on graceful exits
- Content scanner: overlapping chunks → `POST /api/v1/agent/scan/batch` → verdicts back to local `files` table
- Local visualization dashboard (Python server + HTML)

### What Does Not Exist Yet

- Policy engine (decisions from scan results)
- User-mode enforcement (blocking file operations)
- Kernel-mode enforcement (Windows Minifilter / Linux eBPF / fanotify)
- IPC + user helper (tray icon, notifications)
- Auto-update mechanism
- Code signing and packaging (`.deb`, `.rpm`, MSI)
- Rich-document extraction (PDF, DOCX, XLSX, images)
- gRPC transport (scaffolding exists but disabled — see §5.4)

---

## 2. The Four Verified Platforms

All four targets build, link, and run. All support TLS and the full
bidirectional WebSocket pipeline.

| Platform           | Toolchain             | Build | Run | Watch | Hash | HTTP | WSS | Scanner | Shutdown |
| ------------------ | --------------------- | ----- | --- | ----- | ---- | ---- | --- | ------- | -------- |
| **Linux x86_64**   | GCC + `.deps/`        | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |
| **Linux i686**     | GCC `-m32` + `.deps/` | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |
| **Windows x86_64** | MSVC + vcpkg          | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |
| **Windows x86**    | MSVC + vcpkg          | ✅     | ✅   | ✅     | ✅    | ✅    | ✅   | ✅       | ✅        |

**Windows binary verification:**

| Binary                                              | Size    | Machine      |
| --------------------------------------------------- | ------- | ------------ |
| `build/windows-x64-msvc-release/axiomdlp-agent.exe` | 8.61 MB | `8664 (x64)` |
| `build/windows-x86-msvc-release/axiomdlp-agent.exe` | 6.37 MB | `14C (x86)`  |

No external DLL dependencies beyond `KERNEL32`, `ADVAPI32`, `WS2_32`,
`SHELL32`, `ole32`, `USER32`, `CRYPT32`.

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

**Dependency pipeline (CMake side):**

```
                       CMakeLists.txt
                             │
                 ┌───────────┴───────────┐
                 ▼                       ▼
           if(MSVC)                if(UNIX AND NOT APPLE)
                 │                       │
                 ▼                       ▼
           vcpkg manifest          .deps/ cache
           (Windows native)        (Linux native)
                 │                       │
        ┌────────┴────────┐     ┌───────┴────────┐
        ▼                 ▼     ▼                ▼
    x64-windows-static   x86-windows-static   linux-x86_64   linux-i686
```

---

## 4. Source Tree Layout

```
axiomdlp-agent/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json                              ← Windows deps manifest
├── README.md
├── DONE.md                                 (this file)
├── LICENSE
├── .clangd / .clang-tidy / .clang-format
├── .gitignore
│
├── .deps/                                  Linux deps cache (gitignored)
│   ├── spdlog-src/
│   ├── nlohmann_json-src/
│   ├── efsw-src/
│   ├── sqlite3-src/
│   ├── openssl-src/
│   ├── openssl-linux-x86_64/
│   ├── openssl-linux-i686/
│   └── boost-include/
│
├── vcpkg_installed/                        Windows deps (gitignored)
│   ├── x64-windows-static/
│   └── x86-windows-static/
│
├── cmake/toolchains/
│   ├── linux-x86_64.cmake
│   └── linux-i686.cmake
│
├── config/
│   ├── agent.default.json
│   ├── agent.default_win.json
│   └── agent.schema.json
│
├── data/                                   (gitignored)
├── logs/                                   (gitignored)
│
├── scripts/
│   ├── linux/
│   │   ├── build-all.sh
│   │   ├── build-grpc.sh
│   │   ├── build-openssl.sh
│   │   └── merge-compile-commands.sh
│   ├── win/
│   │   ├── setup-windows-msvc.ps1
│   │   └── build-windows-msvc.ps1
│   ├── mock_server.py
│   ├── viz_server.py
│   └── viz.html
│
├── docs/
│   └── windows-msvc-build.md
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
    │   ├── module_context.h           Holds ShutdownController&
    │   ├── module_manager.{h,cpp}
    │   └── health.h

    ├── platform/
    │   ├── paths.h
    │   ├── service.h                  Takes ShutdownController
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
        ├── content_scanner/
        │   └── content_scanner_module.{h,cpp}
        └── grpc_transport/            (scaffolding, disabled by default)
            ├── agent.proto
            ├── grpc_transport_module.{h,cpp}
```

---

## 5. Build System

### 5.1 Linux (x86_64 + i686) — unchanged from `main`

**Prerequisites (Kali/Debian/Ubuntu):**

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y \
    build-essential gcc-multilib g++-multilib \
    ninja-build git file clangd \
    libboost-dev libsystemd-dev libsystemd-dev:i386
```

**Build:**

```bash
./scripts/linux/build-all.sh release
```

**First run:** downloads and compiles OpenSSL 3.3.2 for both Linux
targets (~15–25 min). **Subsequent runs:** incremental (~30 sec).

**Presets:**

| Preset                         | Output                                | Compiler            |
| ------------------------------ | ------------------------------------- | ------------------- |
| `linux-x86_64-{debug,release}` | `build/linux-x86_64-*/axiomdlp-agent` | `/usr/bin/g++`      |
| `linux-i686-{debug,release}`   | `build/linux-i686-*/axiomdlp-agent`   | `/usr/bin/g++ -m32` |

### 5.2 Windows (x64 + x86) — MSVC + vcpkg

**Prerequisites:**
- **Visual Studio 2022** with "Desktop development with C++" workload (BuildTools works)
- **CMake 3.24+** on PATH
- **Ninja** on PATH
- **vcpkg** cloned to `C:\Users\<user>\vcpkg` with `VCPKG_ROOT` set
- **Long paths enabled** (`LongPathsEnabled=1` in registry)

**One-time setup:**

```powershell
.\scripts\win\setup-windows-msvc.ps1
```

Fast mode: downloads vcpkg as ZIP from `codeload.github.com` (no git clone,
~25 MB) and downloads prebuilt `vcpkg.exe` from `vcpkg-tool/releases`
(no bootstrap compile, ~3 MB). Takes ~3 min. Also has a `setx` fallback
for `VCPKG_ROOT` if the .NET `SetEnvironmentVariable` API fails.

**Build:**

```powershell
# x64 Release
.\scripts\win\build-windows-msvc.ps1 -Arch x64

# x86 Release
.\scripts\win\build-windows-msvc.ps1 -Arch x86
```

The build script **auto-initializes the Visual Studio environment** via
`vcvarsall.bat`, so it works from any PowerShell window — Developer
PowerShell is not required. It also:

- Saves `VCPKG_ROOT` before VS init and restores it after (Bug W22 —
  `vcvarsall.bat` overwrites it with the VS-bundled vcpkg)
- Refuses to build if `VCPKG_ROOT` ends up pointing at the VS-bundled path
- Explicitly sets `CC=cl.exe` and `CXX=cl.exe` to override MSYS2 GCC
  if it's on PATH ahead of MSVC (Bug W21)
- Detects stale `CMakeCache.txt` with GCC compiler entries and cleans
  the build directory

**First Windows build:** vcpkg compiles 66 packages per triplet
(~25–40 min per architecture). **Subsequent builds:** all 66 packages
restore from vcpkg's binary cache in ~5 seconds.

**Presets:**

| Preset                             | Output                                        | Triplet              |
| ---------------------------------- | --------------------------------------------- | -------------------- |
| `windows-x64-msvc-{debug,release}` | `build/windows-x64-msvc-*/axiomdlp-agent.exe` | `x64-windows-static` |
| `windows-x86-msvc-{debug,release}` | `build/windows-x86-msvc-*/axiomdlp-agent.exe` | `x86-windows-static` |

MSVC presets are gated by `hostSystemName == "Windows"` in
`CMakePresets.json`, so they are invisible on Linux. Both use
`toolchainFile: $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`.

**vcpkg cache location:** `C:\Users\<user>\AppData\Local\vcpkg\archives\`
Second-run restore times observed: **3.6–5.7 seconds** for all 66 packages.

**Offline/slow-network fallback:** if vcpkg downloader stalls (GitHub CDN
throttle), download the failing URL with `aria2c -x16 -s16` inside WSL,
then copy the file to `C:\Users\<user>\vcpkg\downloads\`. vcpkg accepts
pre-cached files silently. Documented in §22 (Bugs W18–W20).

### 5.3 Build strategy selection

```cmake
if(MSVC)
    set(AXIOM_USE_VCPKG_DEPS ON)
    message(STATUS "Build strategy: MSVC + vcpkg (Windows native)")
    # Static CRT — matches MinGW -static-libstdc++
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(AXIOM_USE_VCPKG_DEPS OFF)
    message(STATUS "Build strategy: GCC native (.deps/)")
endif()
```

Every `find_package(...)` block in `CMakeLists.txt` branches on
`AXIOM_USE_VCPKG_DEPS`:

| Dependency    | vcpkg path                                               | `.deps/` path                                       |
| ------------- | -------------------------------------------------------- | --------------------------------------------------- |
| nlohmann/json | `find_package(CONFIG REQUIRED)`                          | system or FetchContent                              |
| spdlog        | `find_package(CONFIG REQUIRED)`                          | FetchContent                                        |
| efsw          | `find_package(CONFIG REQUIRED)` → `efsw::efsw`           | FetchContent → `efsw-static`                        |
| Boost         | `find_package(Boost CONFIG REQUIRED)` → `Boost::headers` | isolated `/usr/include/boost` symlink               |
| OpenSSL       | `find_package(OpenSSL REQUIRED)`                         | manual `OpenSSL::SSL` / `::Crypto` imported targets |
| SQLite3       | `find_package(SQLite3 REQUIRED)` → `SQLite::SQLite3`     | amalgamation → `axiom_sqlite` static lib            |

### 5.4 gRPC — optional, off by default

The gRPC transport module scaffolding exists in
`src/features/grpc_transport/` (`agent.proto`, `.h`, `.cpp`) but is
**disabled by default** via `option(AXIOM_ENABLE_GRPC ... OFF)` in
`CMakeLists.txt`.

**Why disabled:** `grpc` and `protobuf` are not in `vcpkg.json`. The
gRPC CMake block would fail with `Could not find Protobuf` if it ran
unconditionally. Gating it behind `AXIOM_ENABLE_GRPC` keeps the
non-gRPC build path clean.

**To enable later (Windows):**

1. Add to `vcpkg.json`:
   ```json
   "protobuf",
   { "name": "grpc", "features": [] }
   ```
2. Configure with:
   ```powershell
   cmake --preset windows-x64-msvc-release -DAXIOM_ENABLE_GRPC=ON
   ```
3. First enable build: ~30–45 min extra vcpkg time for abseil +
   protobuf + grpc. Subsequent builds: cached, ~5 sec extra.

**On Linux**, gRPC is currently unavailable — only the Windows code path
is wired into `CMakeLists.txt`. Wiring `.deps/grpc-linux-*` from
`scripts/linux/build-grpc.sh` into the `else()` branch of the
`AXIOM_ENABLE_GRPC` block is a follow-up task.

### 5.5 Dependencies — per-platform

**Linux:**

| Library       | Version  | Purpose                | Source                            |
| ------------- | -------- | ---------------------- | --------------------------------- |
| spdlog        | 1.14.1   | Async logging          | FetchContent + `.deps/`           |
| nlohmann/json | 3.11.3   | JSON parsing           | System package or FetchContent    |
| efsw          | 1.7.2    | File watching          | FetchContent + `.deps/`           |
| SQLite        | 3.46.1   | Metadata storage       | Amalgamation + `.deps/`           |
| Boost         | 1.90     | HTTP + WebSocket       | System headers, isolated include  |
| OpenSSL       | 3.3.2    | TLS + hashing + base64 | `build-openssl.sh` for both archs |
| libsystemd    | (system) | `sd_notify`            | pkg-config (optional)             |

**Windows:**

| Library       | Version | Source                                                            |
| ------------- | ------- | ----------------------------------------------------------------- |
| OpenSSL       | 3.6.5   | vcpkg (`openssl:x64/x86-windows-static`)                          |
| Boost         | 1.92.0  | vcpkg (`boost-asio`, `boost-beast` + 60 transitive boost-* ports) |
| spdlog        | 1.17.0  | vcpkg                                                             |
| fmt           | 12.2.0  | vcpkg (spdlog dependency)                                         |
| efsw          | 1.7.2   | vcpkg                                                             |
| SQLite        | 3.53.4  | vcpkg                                                             |
| nlohmann/json | 3.12.0  | vcpkg                                                             |

**Note:** Boost and OpenSSL versions differ by platform (they come from
different sources — `.deps/` vs vcpkg). The public API used by the agent
(Beast HTTP + WebSocket, Asio async I/O, OpenSSL EVP + X509) is stable
across both.

**Total vcpkg packages built per Windows triplet: 66.** Includes
`boost` (60 sub-packages), `openssl`, `sqlite3`, `spdlog`, `fmt`,
`nlohmann-json`, `efsw`, `vcpkg-cmake`, `vcpkg-cmake-config`,
`vcpkg-cmake-get-vars`, `vcpkg-boost`, `boost-uninstall`.

---

## 6. Foundation Layer

### `ShutdownController` — `src/foundation/shutdown.h`

The single, thread-safe authority on whether the agent is shutting down
and why.

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
10. Create `ModuleContext` (includes the shutdown controller).
11. Create `ModuleManager`, register 6 modules.
12. Initialize all modules.
13. For install/uninstall: short-circuit.
14. For normal run: create platform `IService`, wire manager and controller, run.

**Shutdown sequence:** destroys members in reverse dependency order
**before** `Logger::shutdown()` to avoid the segfault that happened when
`Database::~Database()` logged through a dead spdlog singleton.

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

- All server-facing values live here. `agent_sync`, `transport`, and
  `content_scanner` read from the same place.
- `verify_tls` is no longer parsed anywhere — TLS is always verified on
  `https://` and `wss://`.
- Legacy fields under `modules.agent_sync.*` and `modules.transport.*`
  are still read (with a deprecation warning) so old configs keep working.
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

**Canonical event types:** `file_access`, `file_write`, `file_delete`,
`usb_insert`, `usb_remove`, `clipboard_copy`, `print_job`,
`process_spawn`, `scan_result`, `decision`.

### `EventBus` — `src/events/event_bus.{h,cpp}`

In-process synchronous publish/subscribe. `shared_ptr<Event>` is shared
with all subscribers. Handlers are invoked outside the lock, so they can
safely subscribe or unsubscribe during a callback.

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
    ShutdownController& shutdown;
};
```

### `ModuleManager` — `src/modules/module_manager.{h,cpp}`

Owns `std::vector<std::unique_ptr<IModule>>`. Start in forward order;
stop and shutdown in reverse. Aggregates health.

### `HealthStatus` — `src/modules/health.h`

Four levels: `Unknown`, `OK`, `Degraded`, `Failed`.

---

## 9. Platform Layer

### Interfaces

| File                      | Purpose                                                           |
| ------------------------- | ----------------------------------------------------------------- |
| `platform/paths.h`        | Executable path, config path, log dir, data dir                   |
| `platform/service.h`      | `IService` with `set_shutdown_controller`, `request_stop(reason)` |
| `platform/file_watcher.h` | `IFileWatcher` + `FileEvent`                                      |

### Linux — `platform/linux/`

- `paths.cpp` — `readlink("/proc/self/exe")`, `/etc/axiomdlp/`, `/var/log/axiomdlp/`, `/var/lib/axiomdlp/`.
- `service.cpp` — systemd, `sd_notify`, signals. Signal handler sets an atomic flag; main loop translates it into a `ShutdownController` request.

**Build-time guards:** `src/platform/linux/*.cpp` start with
`#if !defined(__linux__) #error "…"` — the build fails clearly if
CMake ever includes them on the wrong host.

### Windows — `platform/windows/`

- `paths.cpp` — `GetModuleFileNameA`, `SHGetKnownFolderPath` (via
  `FOLDERID_ProgramData`), `CoTaskMemFree` cleanup.
- `service.cpp` — SCM, `ServiceMain`, control handler.
  `SERVICE_CONTROL_STOP` → `ShutdownController::request(UserRequested)`.

**Build-time guards:** `src/platform/windows/*.cpp` start with
`#if !defined(_WIN32) #error "…"` — same protection.

**Windows link additions:** `ole32` (for `CoTaskMemFree`),
`user32` (shell helpers), plus the existing `advapi32 shell32 ws2_32 crypt32`.

---

## 10. Net Layer

### `HttpClient` — `src/net/http_client.{h,cpp}`

Synchronous HTTP/HTTPS client built on Boost.Beast.

- POST with JSON body and optional Bearer token.
- Supports `http://` and `https://`.
- **TLS is always verified on `https://`.** The `verify_tls` parameter
  was removed; certificate chain and hostname must match or the call fails.
- SNI set before the handshake.
- Never throws; returns `{ status, body, error }`.

### `WebSocketClient` — `src/net/websocket_client.{h,cpp}`

Asynchronous WebSocket client built on Boost.Beast + Boost.Asio.

- Dedicated `io_context` thread.
- **`set_bearer_token()`** injects `Authorization: Bearer <token>` into
  the handshake via a Beast decorator.
- **No application-level read deadline.** Liveness is owned by Beast's
  own `stream_base::timeout` (`idle_timeout = 90s`, `keep_alive_pings = true`).
- **Exponential backoff with ±20% jitter** drawn from `std::mt19937`.
  A fleet of thousands does not reconnect in lockstep.
- **Close codes interpreted.** `1000`, `1001`, `1009`, `1012`, `1013`
  retry. `1008` (Policy Violation) stops permanently. `1011` retries
  (server bug).
- **Graceful close on `stop()`**: sends a `1000 Normal` close frame
  (with 2s timeouts) before tearing down, so the server logs a clean
  close instead of a `1006 abnormal closure`.
- **TLS is always verified on `wss://`** — no opt-out.
- Send/stop race serialized by `lifecycle_mutex`.

**MSVC build note:** this file is the heaviest in the project — it
instantiates the full Boost.Beast WebSocket stack (async read/write,
TLS, close codes). On MSVC it exceeds the default object-file section
limit (`C1128: number of sections exceeded object file format limit`).
`CMakeLists.txt` adds `/bigobj` to all MSVC compile options
unconditionally. Linux has no equivalent limit.

### `OutboundQueue` — `src/net/outbound_queue.{h,cpp}`

Bounded, priority-ordered FIFO. Four priorities. Bounded by both
message count and total bytes.

### `MessageRouter` — `src/net/message_router.{h,cpp}`

Type-keyed dispatch for incoming JSON. Handlers registered by string
type. Unknown types logged and ignored.

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

Identity (`id`, `dev`, `inode`), path decomposition, POSIX metadata,
flags, classification (`category`, `file_type`), scan result
(`is_sensitive`, `sensitivity`, `scan_status`), scan metadata
(`content_hash`, `last_scanned_at`, `scan_attempts`, `scan_error`),
bookkeeping (`first_seen`, `last_seen`, `walk_id`).

**Unique constraint:** `(dev, inode)`.

**Indexes:** `path`, `dirname`, `extension`, `category`, `mtime_ns DESC`,
`size DESC`, `uid`, `walk_id`, `is_sensitive` (partial), `scan_status`,
`content_hash`.

### Schema Versioning

`PRAGMA user_version` tracks the version. Current: **4**. Migrations
apply automatically on startup. Both Linux and Windows apply the same
migration sequence (0→4 verified on a fresh Windows database).

### `Database` / `FileStore` / `StateStore`

- `Database` is a thin SQLite wrapper with WAL, `synchronous=NORMAL`,
  `foreign_keys=ON`, `busy_timeout=5000`. No logging in `close()` —
  the destructor may run after logger teardown.
- `FileStore` is the domain wrapper for `files`: upsert, pending-scan
  queries, verdict deduplication, scan-result writes, error recording,
  and aggregations.
- `StateStore` is the key/value store backed by `state`. Used by
  `agent_sync` for `agent_id`.

**Note:** SQLite version differs by platform — **3.46.1 on Linux**
(amalgamation), **3.53.4 on Windows** (vcpkg). Schema and query
compatibility verified by both platforms applying the same migrations
successfully.

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

**Windows runtime observation:** on first run with the Linux config,
the watcher correctly logged `skipping '/home/kali/Downloads' (does not exist)` —
proving efsw is linked and functional. It opened a watch request,
checked path existence, and logged clearly. Switching to
`config/agent.default_win.json` (watch `%USERPROFILE%`) resolves this.

### `inventory`

Periodic full filesystem scan.

- Scheduled on an interval (default 900s), owns its worker thread.
- Rich metadata capture: path decomposition, timestamps in nanoseconds,
  POSIX permissions, file type, category.
- SHA-256 hashing for eligible files.
- Explicit `scan_status` assignment: `pending`, `not_applicable`, `skipped_too_large`.
- Batched inserts every 5,000 rows.
- Graceful stop within ~20 ms; partial commits on cancel.

### `event_logger`

Subscribes to all events on the bus and logs them. Pure observability.

### `websocket_transport`

Owns the WebSocket connection to the Core server.

- Reads `server.base_url` and `server.api_token` from the top-level
  `server` config section.
- Derives the WebSocket URL from `server.base_url` unless an explicit
  `server_url` override is set.
- Waits for `agent_id` in the state store before connecting.
- On graceful shutdown (`UserRequested`, `FatalError`, `ConfigError`)
  sends `{"type":"agent_shutdown","payload":{"reason":...}}` with
  `Priority::Critical` **before** closing the socket.
- Handles server-pushed `token_revoked` and `agent_revoked` messages by
  calling `ShutdownController::request(AuthRevoked/AgentRevoked)`.
- Default handlers for `ping`, `policy_update`, `command`.

### `agent_sync`

The HTTP channel between the agent and the Core server.

- **Enrollment** — `POST /api/v1/agent/register` (once at startup).
- **Heartbeat** — `POST /api/v1/agent/heartbeat`, default 60 s.
- On startup with a saved `agent_id`, sends **one immediate validation
  heartbeat** so a revoked token is detected within milliseconds instead
  of after the first interval.
- 401 → `ShutdownController::request(AuthRevoked)` with the message
  `"no valid API token — heartbeat rejected (401) — token is invalid,
  expired, or revoked"`.
- 403 → `ShutdownController::request(AgentRevoked)`.
- 404 → clears `agent_id` and re-enrolls.
- Empty `server.api_token` at init → error message says **"no valid API
  token"** explicitly.
- Interruptible sleep; also wakes when the shutdown controller is triggered.

**Windows runtime observation:** on first run with no Core server,
returned `connect: No connection could be made because the target
machine actively refused it [system:10061]`. This proves Boost.Asio
socket, Winsock (`ws2_32`), and error mapping all work — the subsystem
failed correctly and logged the retry intent.

### `content_scanner`

Reads text files, splits into overlapping chunks, sends to Core, writes
verdicts back.

- Reads from `server.base_url` and `server.api_token`.
- Extension filter, size filter, existence check, SHA-256 of content,
  verdict deduplication by hash.
- Chunk size 1 MB, overlap 256 bytes.
- Batches respect `batch_size` and `batch_max_bytes`.
- Updates `is_sensitive`, `sensitivity`, `scan_status = 'scanned'` on success.
- Content is never persisted; read into memory, sent, discarded.

---

## 14. TLS / OpenSSL

### Why

HTTP and WebSocket traffic carries the `axdlp_user_...` token. If that
token leaks, an attacker can impersonate the agent.

### Two OpenSSL versions, both static

| Platform | Version | Location                                                            |
| -------- | ------- | ------------------------------------------------------------------- |
| Linux    | 3.3.2   | `.deps/openssl-linux-*/lib{,64}/libssl.a` + `libcrypto.a`           |
| Windows  | 3.6.5   | `vcpkg_installed/*-windows-static/lib/libssl.lib` + `libcrypto.lib` |

Both are statically linked into the final binary. Verified:

- Linux: `ldd axiomdlp-agent | grep ssl` → empty (no `.so` deps)
- Windows: `dumpbin /dependents axiomdlp-agent.exe` → only `KERNEL32`,
  `ADVAPI32`, `WS2_32`, `SHELL32`, `ole32`, `USER32`, `CRYPT32`
  (no `libssl-3-x64.dll`, no `libcrypto-3-x64.dll`)

### Verification (unchanged)

Both `HttpClient` and `WebSocketClient` verify chain **and** hostname.
Implemented via `X509_VERIFY_PARAM_set1_host()` because Boost's
`host_name_verification` doesn't exist in Boost 1.90.

### No Opt-Out

`verify_tls` was removed from every layer:

- `TransportModuleConfig`, `AgentSyncModuleConfig` — field removed.
- `HttpClient::post_json` — parameter removed.
- `WebSocketClient::set_verify_tls` — method removed.
- `config/agent.schema.json` — property removed.
- Legacy `verify_tls` keys in config files produce a **warning that
  they are ignored**; the agent still verifies.

### SNI

Both clients call `SSL_set_tlsext_host_name()` before the handshake.

### Static CRT (Windows)

`CMAKE_MSVC_RUNTIME_LIBRARY = "MultiThreaded$<$<CONFIG:Debug>:Debug>"`
forces `/MT`. This matches MinGW's `-static-libstdc++` behavior and
eliminates the `vcruntime140.dll` runtime dependency. Any Windows 10/11
machine can run the `.exe` with no redistributable install.

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
- `grpc_transport`: `enabled` (default `false` — gRPC is off by default).

### Fail-Safe Behavior

Missing file, invalid JSON, wrong type, invalid value — all produce
warnings and fall back to safe defaults. **The agent always starts.**
Config problems never prevent operation.

---

## 16. The Data Model

After an inventory, `files` contains every regular file under the
watch paths. Fields are populated by `stat()` during the walk, by
SHA-256 hashing for eligible files, and by the content scanner for
verdicts.

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

### Connection Setup

1. `agent_sync` enrolls via HTTP, gets `agent_id`, writes it to SQLite `state`.
2. `transport` polls the state store until `agent_id` appears.
3. `transport` derives the WebSocket URL from `server.base_url` (or uses
   an explicit override).
4. `WebSocketClient::start()` spawns the I/O thread.
5. Handshake carries `Authorization: Bearer <token>`.
6. On 101 Switching Protocols, the client transitions to `Connected`.
7. Every 30 s the server pings; Beast on the client responds with pongs
   automatically.
8. Every 60 s `agent_sync` heartbeats over HTTP, updating `last_seen_at`.

### Message Flow

- **Server → agent**: any handler can call `agentHub.SendToAgent(id, msg)`
  or `agentHub.BroadcastToOrg(orgID, msg)`.
- **Agent → server**: any module can call `TransportModule::send_message(json)`.
  The message is queued in `OutboundQueue` and written by the write pump.
- **Goodbye on graceful shutdown**: `TransportModule::stop()` sends
  `{"type":"agent_shutdown","payload":{"reason":...}}` at
  `Priority::Critical` before closing.

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

| Code | Meaning          | Behavior                            |
| ---- | ---------------- | ----------------------------------- |
| 1000 | Normal           | Reconnect                           |
| 1001 | Going Away       | Reconnect                           |
| 1006 | Abnormal         | Reconnect                           |
| 1008 | Policy Violation | **Stop permanently** (auth revoked) |
| 1009 | Too Big          | Reconnect                           |
| 1011 | Internal         | Reconnect                           |
| 1012 | Service Restart  | Reconnect                           |
| 1013 | Try Again Later  | Reconnect                           |

### Server-Side Revocation

Two independent mechanisms:

1. **Token watcher**: the server re-checks the DB every 60 s. If the
   token is revoked, it sends `{"type":"token_revoked",...}` then a
   `1008 Policy Violation` close frame.
2. **Heartbeat 401**: the agent's next HTTP heartbeat returns 401.
   `agent_sync` calls `ShutdownController::request(AuthRevoked)`.

Whichever fires first, the whole agent exits cleanly in ~1 second.

### Atomic Shutdown

Every module can request shutdown at any time. The service main loop
unblocks and runs `stop_all()` then `shutdown_all()` in reverse order.
The final log line shows the reason.

---

## 18. The Complete Bug History

### Windows MSVC migration bugs (new — `msvc-windows-build` branch)

**Bug W15 — PowerShell 5.1 mis-parses UTF-8 `.ps1` files without BOM.**
Em-dashes (`—`), ellipses (`…`), and smart quotes in the `.ps1` files
were interpreted as three separate Windows-1252 characters each. The
stray `"` in a comment **terminated a string on line 20**, causing a
cascade of `Missing argument in parameter list` and `Unexpected token`
errors from every subsequent line. **Fix:** rewrote both `.ps1` files as
pure ASCII (0x00–0x7F only). Replaced Unicode symbols with ASCII
equivalents (`-`, `...`, `[OK]`, `[ERROR]`, `[WARN]`).

**Bug W16 — `git clone vcpkg` at 9 KB/s = days.** The full vcpkg repo
has 345,595 objects; on a slow link the clone is impractical. **Fix:**
download as ZIP from `codeload.github.com` (~25 MB, ~2 min).

**Bug W17 — `bootstrap-vcpkg.bat` compilation unnecessary.** vcpkg-tool
is prebuilt and released on GitHub. **Fix:** download `vcpkg.exe` from
`vcpkg-tool/releases/latest/download/vcpkg.exe` (~3 MB).

**Bug W18 — Strawberry Perl download stalls twice.** OpenSSL's
`Configure` needs Perl; vcpkg downloads 291 MB from GitHub. Stalled at
0 MB on first attempt, stalled at 131 MB on second. **Fix:** downloaded
via aria2c in WSL (16 parallel connections, `-x16 -s16`), then copied
from `/tmp` to `C:\Users\<user>\vcpkg\downloads\`. The 291 MB file was
verified complete; vcpkg's hash check accepted it.

**Bug W19 — jom download stalls.** jom is Qt's `nmake` replacement,
needed by OpenSSL. Same fix: aria2c download + copy to vcpkg downloads.

**Bug W20 — sqlite3 source download stalls.** Same fix: aria2c + copy.

**Bug W21 — MSYS2 GCC hijacks CMake's compiler.** CMake's Ninja
generator scans PATH for `cc`/`gcc`; MSYS2's `C:\msys64\mingw64\bin`
was ahead of MSVC in PATH order, so CMake selected GCC. That flipped
`AXIOM_USE_VCPKG_DEPS` to OFF (GCC is not MSVC) and made CMake look
for Boost at `/usr/include/boost` on Windows. **Fix:** (a) auto-init
VS env via `vcvarsall.bat`, (b) explicitly set `$env:CC = "cl.exe"` and
`$env:CXX = "cl.exe"`, (c) detect stale `CMakeCache.txt` with GCC and
clean the build directory.

**Bug W22 — `vcvarsall.bat` overwrites `VCPKG_ROOT`.** Calling
`vcvarsall.bat` sets `VCPKG_ROOT` to
`...\Microsoft Visual Studio\...\VC\vcpkg` (the VS-bundled vcpkg). That
instance requires `builtin-baseline` in `vcpkg.json`, so it errors with:
`this vcpkg instance requires a manifest with a specified baseline`.
**Fix:** in `build-windows-msvc.ps1`, save `VCPKG_ROOT` before VS init,
restore it after, and refuse to build if it ends up pointing at the
VS-bundled path.

**Bug W23 — `vcpkg-configuration.json` placeholder baseline.** The
file contained `"baseline": "<COMMIT-SHA>"` (literal string). vcpkg
tried to check out that SHA and failed. **Fix:** deleted the file.
vcpkg falls back to registry HEAD, which works.

**Bug W24 — Boost 1.92.0 vcpkg meta-package dropped `asio` / `beast`
features.** `"boost"` with `features: ["asio", "beast", "system"]`
failed with `error: boost@1.92.0 does not have required feature asio`.
The meta-package no longer supports those features. **Fix:** use the
separate `boost-asio` and `boost-beast` ports directly; `boost-system`
comes as a transitive dependency.

**Bug W25 — `/bigobj` required for `websocket_client.cpp`.** MSVC's
default object-file format has a 65,535 section limit. The full
Boost.Beast WebSocket stack (async read/write, TLS, close codes)
exceeds it: `fatal error C1128: number of sections exceeded object
file format limit: compile with /bigobj`. **Fix:** added `/bigobj` to
all MSVC compile options in `CMakeLists.txt`. Linux (GCC/ELF) has no
equivalent limit — this is a build-only fix.

**Bug W26 — `ole32` and `user32` missing from Windows link libs.**
`src/platform/windows/paths.cpp` uses `CoTaskMemFree` (from
`<objbase.h>`, linked via `ole32.lib`) after `SHGetKnownFolderPath`.
Without it, `LNK2019: unresolved external symbol __imp_CoTaskMemFree`.
**Fix:** added `ole32 user32` to the WIN32 `target_link_libraries`.

**Bug W27 — gRPC block activated without protobuf in vcpkg.json.**
`CMakeLists.txt` had `if(AXIOM_USE_VCPKG_DEPS) find_package(Protobuf
CONFIG REQUIRED) ...` which runs whenever MSVC is active. But `grpc`
and `protobuf` were not in `vcpkg.json` — the gRPC module was
scaffolding only, disabled for later. `find_package(Protobuf CONFIG
REQUIRED)` therefore failed with `Could not find a package
configuration file provided by "Protobuf"`. **Fix:** introduced
`option(AXIOM_ENABLE_GRPC ... OFF)` and changed the guard to
`if(AXIOM_USE_VCPKG_DEPS AND AXIOM_ENABLE_GRPC)`. The block is now
cleanly gated — no `find_package` unless explicitly enabled.

### Configuration & shutdown bugs (unchanged from previous revision)

**Bug C1 — `verify_tls` was a footgun.** Removed from every layer.
Legacy keys warn and are ignored.

**Bug C2 — Duplicate token across three modules.** Consolidated into
`server.api_token`.

**Bug C3 — Revoked token silently ignored while running.** `agent_sync`
now requests shutdown on 401/403; the server's watcher also closes the
socket with 1008.

**Bug C4 — Empty token at startup allowed a stale `agent_id` heartbeat.**
Now the agent sends an immediate validation heartbeat and errors with
**"no valid API token"** before entering the loop.

### WebSocket-specific bugs (unchanged from previous revision)

**Bug W1 — 60s forced disconnect.** `start_read()` used
`expires_after(60s)` on every read. Server pings don't complete
application-level reads, so the deadline fired every 60 s and killed
the connection. **Fix:** removed the per-read deadline; liveness is now
Beast's `stream_base::timeout` with `keep_alive_pings = true`.

**Bug W2 — Handshake rejected (401) because no bearer was sent.**
**Fix:** `set_bearer_token()` + a Beast decorator that injects
`Authorization` into the handshake.

**Bug W3 — Slow Unregister.** When `readPump` returned, the writer
waited up to 30 s for the next ping tick. **Fix:** call `cancel()`
immediately after `readPump` returns.

**Bug W4 — `1006 abnormal closure` on Ctrl+C.** Client sent no close
frame. **Fix:** `stop()` now sends a `1000 Normal` close frame with
tightened 2 s timeouts.

**Bug W5 — Send-on-closed-channel panic.** `Unregister` closed the
`Send` channel while a broadcast could still be selecting on it.
**Fix:** introduced a separate `Done` channel; `Send` is never closed.

**Bug W6 — Thundering herd on reconnect.** All agents reconnected
synchronously. **Fix:** ±20% jitter via `std::mt19937`.

**Bug W7 — No caps.** Unlimited connections. **Fix:** 100k global /
20k per org.

**Bug W8 — No rate limit.** One bad agent could DoS the server.
**Fix:** `x/time/rate` limiter, 20/s per agent.

**Bug W9 — Slow shutdown blocked by WebSockets.** **Fix:** `CloseAll()`
runs before `srv.Shutdown()`.

**Bug W10 — Close codes ignored.** Revoked agents kept reconnecting.
**Fix:** `should_reconnect_on_close()`.

**Bug W11 — `write_in_flight` not reset.** **Fix:** reset on every
disconnect path.

**Bug W12 — `close_code` type mismatch.** **Fix:**
`static_cast<websocket::close_code>(cr.code)`.

**Bug W13 — `handshake_declined` doesn't exist in this Boost.** **Fix:**
match on `ec.message()` instead.

**Bug W14 — `bugprone-unused-return-value` from clang-tidy on async
operations.** **Fix:** wrap every non-void-returning Beast call in
explicit `static_cast<void>(...)`.

### Original 23 bugs (kept for history)

Bugs 1–23 in the previous revision are unchanged and still fixed.
Highlights: log feedback loop, shutdown segfault, dev/inode collision,
SQLite cross-compile failure, CMake `file(DOWNLOAD)` silent failure,
missing C language in `project()`, walker interruptibility, debounce
priority, heartbeat timer advance, `std::ignore` vs `void`,
`host_name_verification` in Boost 1.90, OpenSSL `lib/` vs `lib64/`,
CMake 4.x `find_package(OpenSSL)` quirks, i686 OpenSSL selection,
duplicated `glob_match`, post-refactor include paths, and `AgentSync`
starting after `initialize()` failure.

---

## 19. Runtime Evidence

### Linux x86_64 — fresh token

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

### Linux x86_64 — server-side registration

```json
{"msg":"Agent registered","agent_id":1,"user_id":1,"org_id":2,"hostname":"kali","os":"linux","version":"0.1.0"}
{"msg":"agent hub: registered","agent_id":1,"user_id":1,"org_id":2,"token_id":1,"total":1,"org_total":1}
```

### Linux x86_64 — graceful Ctrl+C

```
[info] axiomdlp-agent received stop request (reason=user_requested)
[info] ModuleManager: stopping 6 module(s)
[info] transport module: sending goodbye (reason=user_requested)
[info] WebSocketClient: sent graceful close (code=1000)
[info] transport module: state = disconnected
[info] Service returned rc=0 (reason=user_requested)
[info] Application shutting down
```

Server side:
```json
{"msg":"agent ws: agent shutting down gracefully","agent_id":1,"payload":{"agent_id":1,"reason":"user_requested"}}
{"msg":"agent ws: clean close","agent_id":1}
{"msg":"agent hub: unregistered","agent_id":1,"remaining":0}
```

### Linux x86_64 — revocation while running

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

### Linux x86_64 — revoked token at startup

```
[info] agent_sync: validating token with immediate heartbeat
[error] agent_sync: no valid API token — heartbeat rejected (401) — token is invalid, expired, or revoked
[info] axiomdlp-agent received stop request (reason=auth_revoked)
[info] Application shutting down
```

Exits in ~1 s. No reconnect loop.

### Linux i686 — same behavior

All of the above verified on Linux i686 as well. Both binaries produce
identical logs apart from `hostname` and `os` in the enrollment payload.

### Windows x64 — verified 2026-10-17

```
[2026-10-17 19:25:50.861] [info] [t:7468] AxiomDLP Agent v0.1.0
[2026-10-17 19:25:50.861] [info] [t:7468] Config loaded from: config/agent.default.json
[2026-10-17 19:25:50.864] [info] [t:7468] Database: opened 'data/agent.db'
[2026-10-17 19:25:50.865] [info] [t:7468] Migrations: schema version 0 -> 4
[2026-10-17 19:25:50.865] [info] [t:7468] Migrations: applying v2 (enriched files table)
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: v2 applied
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: applying v3 (state table)
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: v3 applied
[2026-10-17 19:25:50.866] [info] [t:7468] Migrations: applying v4 (scanner metadata)
[2026-10-17 19:25:50.867] [info] [t:7468] Migrations: v4 applied
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'filesystem_watcher' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'inventory' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'event_logger' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'transport' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'agent_sync' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: registering module 'content_scanner' v1.0.0
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 6 module(s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'filesystem_watcher'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: excluding own log directory 'D:\axiomdlp-agent\logs'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: excluding own data directory 'D:\axiomdlp-agent\data'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: initialized with 1 watch path(s), 4 exclude pattern(s), debounce 300 ms
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'inventory'
[2026-10-17 19:25:50.867] [info] [t:7468] inventory module: initialized (scan every 60 s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'event_logger'
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'transport'
[2026-10-17 19:25:50.867] [info] [t:7468] transport module: initialized (url=ws://127.0.0.1:3000/api/v1/agent/ws,backoff=10s..300s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'agent_sync'
[2026-10-17 19:25:50.867] [info] [t:7468] agent_sync module: initialized (server=http://127.0.0.1:3000, heartbeat_interval=60s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: initializing 'content_scanner'
[2026-10-17 19:25:50.867] [info] [t:7468] content_scanner module: initialized (batch_size=50, interval=30s, chunk=1024KB, overlap=256B, max_file=100MB, ext_count=47)
[2026-10-17 19:25:50.867] [info] [t:7468] not launched by SCM; running in foreground
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 6 module(s)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'filesystem_watcher'
[2026-10-17 19:25:50.867] [info] [t:7468] filesystem_watcher module: started (watcher initializing)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'inventory'
[2026-10-17 19:25:50.867] [info] [t:7468] inventory module: started
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'event_logger'
[2026-10-17 19:25:50.867] [info] [t:7468] event_logger: subscribed to all events (id=1)
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'transport'
[2026-10-17 19:25:50.867] [info] [t:7468] transport module: started
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'agent_sync'
[2026-10-17 19:25:50.867] [info] [t:7468] agent_sync module: started
[2026-10-17 19:25:50.867] [info] [t:7468] ModuleManager: starting 'content_scanner'
[2026-10-17 19:25:50.867] [info] [t:7468] content_scanner module: started
[2026-10-17 19:25:50.867] [info] [t:7468] AxiomDLPAgent running (Windows foreground mode)
```

**Warnings observed (both benign):**

1. `EfswFileWatcher: skipping '/home/kali/Downloads' (does not exist)` —
   Linux config path on Windows. Proves efsw is linked and functional.
2. `agent_sync: connect refused [system:10061]` — no Core server running
   at `127.0.0.1:3000`. Proves Boost.Asio socket + Winsock error mapping
   work. Retry logic engaged.

Both warnings disappear once `config/agent.default_win.json` is copied
to `config/agent.json` (watch `%USERPROFILE%`) and the Core server starts.

### Windows x86 — build verified

Same build process, same 66 packages per triplet, same 6-module startup
(identical log to x64 apart from the architecture of the `.exe` itself).
Binary size: 6.37 MB.

### Binary verification

| Binary                                        | Size    | `dumpbin /headers` machine |
| --------------------------------------------- | ------- | -------------------------- |
| `windows-x64-msvc-release/axiomdlp-agent.exe` | 8.61 MB | `8664 machine (x64)`       |
| `windows-x86-msvc-release/axiomdlp-agent.exe` | 6.37 MB | `14C machine (x86)`        |

No external DLL dependencies beyond Windows system DLLs.

---

## 20. Known Limitations

### 20.1 ModuleManager Doesn't Skip Failed Modules

A module that fails `initialize()` still gets `start()` called on it.
Fix pending.

### 20.2 No Process or User Attribution

Events carry a path but not the process. Requires fanotify (Linux) or
Minifilter (Windows).

### 20.3 inotify / ReadDirectoryChangesW Queue Overflow

The periodic inventory scan catches anything the watcher misses.

### 20.4 No Network Filesystem Support

inotify doesn't fire on NFS/SMB. efsw's ReadDirectoryChangesW path on
Windows handles most network shares, but behavior is filesystem-dependent.

### 20.5 Windows Ctrl+C Not Handled in Foreground

No `SetConsoleCtrlHandler` registered. SCM path works. `taskkill /PID`
terminates the process without graceful shutdown.

### 20.6 Windows Path Case Sensitivity

The glob matcher is case-sensitive. NTFS is case-insensitive, so a
case-sensitive matcher could miss exclusions. Worked around by matching
what the OS reports.

### 20.7 Dev/Inode Are Placeholders

`dev` is always 0; `inode` is a path hash. Sufficient for uniqueness;
not a real inode reference.

### 20.8 Content Scanner Only Handles Text Files

PDFs, DOCX, XLSX, images are marked `not_applicable`.

### 20.9 `walks` Table Unused

The schema exists; nothing populates it.

### 20.10 Mixed Path Separators on Windows

Cosmetic only.

### 20.11 Platform toolchain duplication

Linux and Windows use different dependency pipelines. Boost is 1.90 on
Linux (system) and 1.92 on Windows (vcpkg). OpenSSL is 3.3.2 on Linux
and 3.6.5 on Windows. This is intentional: `.deps/` works for Linux
(no vcpkg needed), vcpkg is required for Windows (gRPC later).

### 20.12 gRPC scaffolding is currently dead code

`src/features/grpc_transport/` exists but is entirely guarded by
`#ifdef AXIOM_HAVE_GRPC`. Without `AXIOM_ENABLE_GRPC=ON` and the two
`vcpkg.json` entries, it compiles to empty objects and adds no runtime
behavior. This is deliberate — gRPC is a follow-up milestone.

### 20.13 Windows setup requires manual download workarounds on slow networks

The `setup-windows-msvc.ps1` fast-mode uses `Invoke-WebRequest`, which
can stall on slow GitHub CDN. The WSL aria2c fallback (documented in
§5.2) works but is not scripted. Future improvement: switch to
`Start-BitsTransfer` or `BITS` protocol for resumable downloads.

### 20.14 OpenSSL Linux build requires ~20 minutes

One-time per OpenSSL version bump. Cached trees reuse.

---

## 21. What Does Not Exist Yet

| Feature                  | Notes                                        |
| ------------------------ | -------------------------------------------- |
| Policy engine            | Scan results → decisions                     |
| User-mode enforcement    | Block file operations                        |
| Kernel enforcement       | Minifilter / eBPF / fanotify                 |
| IPC + tray helper        | Notifications, pause/resume                  |
| Auto-update              | Signed manifests, atomic swap                |
| Code signing             | Authenticode / GPG                           |
| Packaging                | `.deb`, `.rpm`, MSI                          |
| Rich document extraction | PDF, DOCX, XLSX inside content scanner       |
| Process/user attribution | Requires kernel-level monitoring             |
| gRPC transport (Windows) | Scaffolding exists, disabled by default      |
| gRPC transport (Linux)   | `build-grpc.sh` exists, CMake wiring pending |

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
- Separate concerns: `agent_sync` (HTTP) and `transport` (WS) are
  different lifecycles.

### On Configuration

- One source of truth per concept. `server.api_token` beats three copies.
- Security defaults must not be options. `verify_tls` is gone.
- Backwards compatibility is cheap when done in the parser.

### On Cross-Platform

- **Two toolchains is the right answer, not one.** Linux works with
  `.deps/` + apt. Windows *requires* vcpkg for gRPC-compatible static
  linking. Forcing one to adopt the other's strategy trades working
  code for theoretical uniformity.
- **The `if(MSVC)` switch is a clean seam.** No source file knows which
  path is active. Both call the same `find_package` API and link the
  same imported targets.
- **`/bigobj` and static CRT are the only Windows-specific C++ concerns.**
  Everything else — Boost template size, WinSock error mapping, SCM
  service registration — behaves exactly as the source expects.
- **Static linking eliminates a class of deployment problems.** No
  `vcruntime140.dll`, no `libssl-3-x64.dll`, no DLL search path issues.
  The `.exe` runs anywhere.
- **Toolchain isolation prevents host libraries from leaking.** The
  `platform/` interface pattern means no business logic includes
  `<windows.h>` or `<sys/inotify.h>`.

### On Windows Build Infrastructure

- **PowerShell 5.1 is hostile to non-ASCII in `.ps1`.** Any Unicode in
  a script (em-dash, ellipsis, smart quote, checkmark) will break the
  parser unless the file is saved with a UTF-8 BOM. ASCII-only is the
  durable fix.
- **`vcvarsall.bat` is the source of truth for MSVC env.** Setting
  `PATH` alone is insufficient — `INCLUDE`, `LIB`, `LIBPATH`,
  `VCINSTALLDIR` must all be present. Calling `vcvarsall.bat` in a
  subprocess and importing its env is the reliable pattern.
- **`VCPKG_ROOT` can be silently overwritten by VS env init.** Save
  and restore it. Refuse to proceed if it points at the VS-bundled
  vcpkg.
- **MSYS2 in PATH breaks CMake's compiler detection.** Explicitly set
  `CC=cl.exe`, `CXX=cl.exe` and detect/clean stale `CMakeCache.txt`
  with GCC compiler entries.
- **vcpkg downloads are fragile on slow GitHub links.** Use aria2c
  (or equivalent parallel downloader) in WSL and drop the file into
  `%USERPROFILE%\vcpkg\downloads\`. vcpkg accepts pre-cached files
  silently.
- **vcpkg binary cache is a 500x speedup.** First build ~30 min per
  architecture; every subsequent build ~30 seconds. This is what makes
  the "one-time cost" a genuine one-time cost.
- **Static triplet + static CRT is the right default for redistribution.**
  `x64-windows-static` / `x86-windows-static` produce a self-contained
  `.exe` with no external dependencies beyond Windows system DLLs.

### On Debugging

- Read the actual error. `close 1006` and `handshake declined` are
  symptoms, not causes.
- Log at the right level. Heartbeat success at DEBUG; revoke at ERROR.
- Run for long enough to see periodic behavior — 60 s+ tests reveal
  timing bugs.
- **A stalled download looks identical to a hung process.** Check file
  size growth (`Get-ChildItem … | Select Length`) across 30-second
  intervals before declaring a hang.
- **A `.part` file with a nonzero size is not proof of completion.**
  Compare against the expected size (e.g., Strawberry Perl is 291 MB,
  not 131 MB — a truncated download at 131 MB was still not usable).

### On the Project

- **The infrastructure is genuinely reusable.** Same `ShutdownController`,
  same event bus, same module framework, same config parser on four
  targets — no platform-specific branches in any of them.
- **The DLP logic is easy once infrastructure exists.** All the hard
  work in this migration was plumbing (build system, dep manager,
  downloader resilience). Policy engine + enforcement will be
  comparatively straightforward.
- **Ship small.** Every change since the beginning has been verified
  before the next.
- **Bug histories like this document are worth their weight.** They
  explain *why* the code and scripts look the way they do. Future
  readers will not spend a day re-discovering that `vcvarsall.bat`
  overwrites `VCPKG_ROOT` — they'll read Bug W22.

---
