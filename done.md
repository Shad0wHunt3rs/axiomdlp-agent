# AxiomDLP Endpoint Agent — Current Status

A modular, cross-platform Data Leak Prevention (DLP) endpoint agent for Linux and Windows, written in C++20.

---

## What's Working Today

| Component | Status | Notes |
|-----------|--------|-------|
| Cross-compilation setup | ✅ Working | Builds for Linux 64/32-bit and Windows 64/32-bit from a single Linux host |
| Async logging | ✅ Working | spdlog-based, console + rotating file, thread-safe |
| Config loading | ✅ Working | JSON-based, fail-safe with defaults, structured for extension |
| Service lifecycle | 🚧 Skeleton only | Empty files created; implementation is next |
| Module system | ⏳ Not started | Planned after service lifecycle |
| Event bus | ⏳ Not started | |
| Policy engine | ⏳ Not started | |
| Transport layer | ⏳ Not started | |

**Current behavior:** The agent starts, loads config, initializes logging, logs a few messages, and exits cleanly. It does not yet run as a service or monitor anything.

---

## Project Structure

```
axiomdlp-agent/
├── CMakeLists.txt                        Root build file
├── CMakePresets.json                     8 build presets (4 targets × debug/release)
├── .gitignore
├── LICENSE
├── README.md
│
├── cmake/
│   └── toolchains/                       Cross-compilation toolchains
│       ├── linux-x86_64.cmake
│       ├── linux-i686.cmake
│       ├── windows-x86_64.cmake
│       └── windows-i686.cmake
│
├── config/
│   ├── agent.default.json                Runtime config (logging section)
│   └── agent.schema.json                 JSON schema for editors/CI
│
├── docs/                                 Architecture reference documents
│
├── packaging/
│   ├── linux/systemd/                    systemd unit file (empty, next step)
│   └── windows/                          SCM install script (empty, next step)
│
├── scripts/
│   └── build-all.sh                      Build all four targets
│
└── src/
    ├── main.cpp                          Entry point
    │
    ├── core/                             Framework (platform-agnostic)
    │   ├── logger.h / .cpp               Async logging
    │   ├── config.h / .cpp               Config loading
    │   ├── application.h / .cpp          Lifecycle owner (skeleton)
    │   ├── service.h                     Service interface (empty)
    │   └── paths.h                       Default paths interface (empty)
    │
    └── platform/                         OS-specific implementations
        ├── linux/
        │   ├── service_linux.cpp         systemd impl (empty)
        │   └── paths_linux.cpp           Linux paths (empty)
        └── windows/
            ├── service_win.cpp           SCM impl (empty)
            └── paths_windows.cpp         Windows paths (empty)
```

---

## What Each Subsystem Does

### Logging (`src/core/logger.h`, `logger.cpp`)

- **Async, thread-safe** logging via spdlog.
- **Two sinks:** colored console output + rotating file (`logs/agent.log`).
- **Rotation:** 5 MiB per file, 3 backups (max ~20 MiB on disk).
- **Levels:** trace, debug, info, warn, error, critical, off.
- **Flush on warn+:** important events survive crashes.
- **Never blocks the caller:** messages are queued and written by a background thread.

### Config (`src/core/config.h`, `config.cpp`)

- **JSON-based** configuration read at startup.
- **Nested sections** mirror the JSON structure.
- **Fail-safe:** missing fields use defaults; invalid values are logged and replaced.
- **Warning collection:** problems are gathered and replayed after logging is initialized.
- **Extensible:** adding a new section follows a mechanical 5-step recipe.

Current config (three fields):

```json
{
  "logging": {
    "dir":   "logs",
    "file":  "agent.log",
    "level": "info"
  }
}
```

---

## How to Build

### Prerequisites (on Kali / Debian / Ubuntu)

```bash
sudo apt install -y \
    build-essential gcc-multilib g++-multilib \
    mingw-w64 ninja-build git file
```

### Build a single target

```bash
# Linux 64-bit release
cmake --preset linux-x86_64-release
cmake --build --preset linux-x86_64-release
```

### Build all four targets

```bash
./scripts/build-all.sh release
```

### Available targets

| Preset | Binary |
|--------|--------|
| `linux-x86_64-release` | `build/linux-x86_64-release/axiomdlp-agent` (ELF 64) |
| `linux-i686-release` | `build/linux-i686-release/axiomdlp-agent` (ELF 32) |
| `windows-x86_64-release` | `build/windows-x86_64-release/axiomdlp-agent.exe` (PE32+) |
| `windows-i686-release` | `build/windows-i686-release/axiomdlp-agent.exe` (PE32) |

Windows binaries are **statically linked** — no DLLs required.

### Run

```bash
cd ~/axiomdlp-agent
./build/linux-x86_64-debug/axiomdlp-agent
```

---

## Dependencies

| Library | Purpose | Source |
|---------|---------|--------|
| **spdlog** | Logging | FetchContent (cached in `.deps/`) |
| **nlohmann/json** | Config parsing | System package (`nlohmann-json3-dev`) with FetchContent fallback |

---

## Design Principles

1. **Platform-agnostic core.** Business logic never includes OS headers.
2. **Platform abstraction layer.** One interface, two implementations (Linux/Windows). CMake picks one at build time.
3. **Fail-safe.** Missing or invalid config uses defaults; the agent always starts.
4. **Never block the event pipeline.** Async logging, `overrun_oldest` policy.
5. **Extensible by design.** New config sections, new modules, and new OS features each follow a fixed recipe.
6. **Explicit over implicit.** No globals, no magic, no hidden state.

---

## What Comes Next

In order:

1. **Service lifecycle** (in progress) — run as systemd service on Linux, SCM service on Windows.
2. **Application class** — replace inline `main.cpp` logic with a lifecycle owner.
3. **Platform paths** — absolute default paths per OS (`/etc/axiomdlp` vs `C:\ProgramData\AxiomDLP`).
4. **Module system** — `IModule` interface, `ModuleManager`, registration.
5. **Event bus** — publish/subscribe between modules.
6. **First module** — filesystem watcher (Linux: inotify, Windows: ReadDirectoryChangesW).
7. **Scanner** — regex-based PII detection.
8. **Policy engine** — match scan results to policies, produce decisions.
9. **Enforcement** — user-mode blocking.
10. **Storage** — SQLite for events, policies, and queue.
11. **Transport** — connect to the core API.

---

## References

- `docs/architecture.md` — deeper design notes
- `docs/modules.md` — module contract and examples
- `docs/event_schema.md` — event data model
- `docs/policy_format.md` — policy JSON format
- `docs/ipc_protocol.md` — service ↔ helper communication

---

## License

Apache 2.0 — see `LICENSE`.

---

**Status:** Foundation complete. Service lifecycle implementation begins next.