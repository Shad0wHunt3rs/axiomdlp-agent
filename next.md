# AxiomDLP Endpoint Agent — Phase 2 README

**Version:** 1.0.0  
**Status:** Design Document + Implementation Roadmap  
**Scope:** Agent-side only. Server changes will be documented separately.

---

## Table of Contents

1. [Current State](#1-current-state)
2. [Phase 2 Goals](#2-phase-2-goals)
3. [Architecture Additions](#3-architecture-additions)
4. [Milestone 15 — Policy Engine](#4-milestone-15--policy-engine)
5. [Milestone 16 — User-Mode Enforcement](#5-milestone-16--user-mode-enforcement)
6. [Milestone 17 — Extended Content Extraction](#6-milestone-17--extended-content-extraction)
7. [Milestone 18 — Advanced Detection](#7-milestone-18--advanced-detection)
8. [Milestone 19 — IPC & Tray Helper](#8-milestone-19--ipc--tray-helper)
9. [Milestone 20 — Kernel Enforcement (Linux eBPF)](#9-milestone-20--kernel-enforcement-linux-ebpf)
10. [Milestone 21 — Kernel Enforcement (Windows Minifilter)](#10-milestone-21--kernel-enforcement-windows-minifilter)
11. [Milestone 22 — Remote Commands & Auto-Update](#11-milestone-22--remote-commands--auto-update)
12. [Milestone 23 — Platform Expansion](#12-milestone-23--platform-expansion)
13. [Milestone 24 — Observability & Telemetry](#13-milestone-24--observability--telemetry)
14. [Milestone 25 — Hardening & Tamper Resistance](#14-milestone-25--hardening--tamper-resistance)
15. [Testing Strategy](#15-testing-strategy)
16. [Build & Release](#16-build--release)
17. [Working Agreement](#17-working-agreement)

---

## 1. Current State

### What Works

- Cross-platform service (Linux x86_64/i686, Windows x86_64/i686)
- Real-time filesystem watching (efsw / inotify / ReadDirectoryChangesW)
- Periodic inventory + SHA-256 hashing
- WebSocket + HTTP with TLS to the Core
- Content scanner for text files (chunked upload)
- SQLite with schema versioning (v4)
- Event bus, module framework, health reporting
- Bearer token auth, jittered reconnect, close-code handling
- Graceful shutdown, auth-failure shutdown

### What Does Not Exist

- No local policy engine
- No enforcement of any kind (block/allow/quarantine)
- No PDF/DOCX/OCR extraction (text files only)
- No fingerprinting (EDM, PDM, document registry)
- No user interface (tray, notifications, IPC)
- No kernel-level hooks
- No remote command channel
- No auto-update
- No macOS, ARM64
- No metrics endpoint
- No tamper resistance

---

## 2. Phase 2 Goals

By the end of Phase 2, the agent must:

1. **Decide locally** — evaluate events against a cached policy set within 5 ms.
2. **Enforce** — block file writes/reads, clipboard copy, print jobs, USB writes.
3. **See more** — extract text from PDFs, DOCX, XLSX, PPTX, images (OCR).
4. **Match more** — exact file fingerprints, document fingerprints, structured data.
5. **Talk to the user** — tray icon, notifications, pause/resume, override prompt.
6. **Survive attack** — anti-kill, service watchdog, signed updates.
7. **Talk to the fleet** — receive remote commands, apply policy updates without restart.
8. **Report health** — Prometheus-style metrics for every subsystem.

**Non-goals for Phase 2:** macOS support, mobile, cloud/SaaS DLP, CASB, eDiscovery. Those are Phase 3+.

---

## 3. Architecture Additions

New modules and interfaces:

```
+-------------------------------------------------------------+
|                     APPLICATION                              |
|                                                              |
|  Existing:  Config, Logger, Database, EventBus, ModuleMgr    |
|  New:       PolicyStore, EnforcerRegistry, IPCServer,        |
|             UpdateManager, MetricsRegistry, Watchdog         |
+--------------------------+----------------------------------+
                           |
        +------------------+------------------+
        |                  |                  |
        v                  v                  v
+---------------+  +----------------+  +----------------+
| Policy Engine |  | Enforcement    |  | Content        |
| (local cache) |  | (user + kernel)|  | Extractors     |
+-------+-------+  +--------+-------+  +--------+-------+
        |                   |                   |
        v                   v                   v
+-------------------------------------------------------------+
|                    EVENT BUS                                 |
+-------------------------------------------------------------+
        ^                   ^                   ^
        |                   |                   |
+-------+-------+  +--------+-------+  +--------+-------+
| FS Watcher    |  | Clipboard     |  | Print Monitor  |
| (existing)    |  | Monitor       |  | (new)          |
+---------------+  +----------------+  +----------------+
```

**New top-level directories:**

```
src/
├── policy/          # Policy engine, cache, evaluator
├── enforcement/     # User-mode enforcers (files, clipboard, print)
├── extractors/      # PDF, DOCX, XLSX, PPTX, image extractors
├── fingerprint/     # EDM, PDM, document registry
├── ipc/             # Unix socket / named pipe server
├── updater/         # Auto-update with signature verification
├── telemetry/       # Metrics registry + HTTP endpoint
├── watchdog/        # Self-monitoring + restart
└── platform/
    ├── linux/
    │   ├── ebpf/    # eBPF loader + programs
    │   └── fanotify/# fanotify backend
    └── windows/
        └── minifilter/ # Kernel driver + user-mode communicator
```

**New helper binary:**

```
axiomdlp-helper/     # Tray icon + notifications
```

---

## 4. Milestone 15 — Policy Engine

**Goal:** Evaluate file events against a local policy set in under 5 ms, with no network dependency.

### 4.1 Deliverables

- `PolicyStore` — SQLite-backed cache of server-provided policies
- `PolicyEngine` — evaluates `(event, scan_result)` → `Decision`
- `Decision` struct — action, risk score, matched policy ID, reason
- Policy version tracking + atomic hot-swap on update
- Subscribes to `scan_result` events on the bus

### 4.2 Data Types

```cpp
// src/policy/policy.h

enum class PolicyAction {
    Allow,      // no action, log at debug
    Log,        // write to audit log, no user impact
    Alert,      // log + admin notification
    Coach,      // log + user educational popup
    Block,      // deny the operation
    Quarantine, // move file to encrypted vault
    Encrypt,    // encrypt file in place
    Redact,     // remove sensitive content
};

struct PolicyRule {
    std::string id;
    std::string name;
    int         priority;         // higher wins
    bool        enabled;

    // Match conditions (all must be true)
    std::vector<std::string> entity_types;   // "CREDIT_CARD", "SSN", ...
    int         min_risk_score = 0;          // 0-10
    double      min_confidence = 0.0;        // 0.0-1.0
    std::vector<std::string> file_extensions; // empty = any
    std::vector<std::string> path_patterns;   // glob, empty = any
    std::vector<std::string> event_types;     // "file_write", ...

    // Action
    PolicyAction action;
    bool         enforce_sync = true;         // block inline
    int          priority_override = 0;
};

struct Decision {
    PolicyAction action       = PolicyAction::Allow;
    int          risk_score   = 0;
    std::string  reason;
    std::string  policy_id;
    std::string  policy_name;
    bool         enforce_sync = false;
};
```

### 4.3 PolicyStore

- Backed by SQLite table `policies` (migration v5)
- `load_from_server()` — downloads signed bundle via HTTPS, verifies signature, atomically replaces
- `get_rules()` — returns an immutable snapshot (shared_ptr to vector)
- `version()` — monotonic counter for change detection
- Thread-safe; readers never block on writers

### 4.4 PolicyEngine

```cpp
class PolicyEngine {
public:
    Decision evaluate(const Event& event, const ScanResult& scan);

private:
    std::shared_ptr<const std::vector<PolicyRule>> rules_;
};
```

Algorithm:
1. Iterate rules in descending priority.
2. For each rule, check every condition against the event + scan.
3. First matching rule wins.
4. No match → `Decision{Allow}`.

### 4.5 Hot-Swap

When the server pushes a policy update via WebSocket:

1. `transport` module receives `policy_update` message with a signed bundle URL + hash.
2. `policy` module downloads the bundle in the background.
3. Verifies Ed25519 signature against an embedded public key.
4. Opens a new SQLite read transaction, writes to `policies_new`, commits.
5. Atomically swaps `rules_` shared_ptr.
6. Publishes `policy_changed` event so other modules can react.

### 4.6 Success Criteria

1. 1000 rules evaluate in < 5 ms on the target hardware.
2. A policy update takes effect within 100 ms of the push.
3. Signed bundle with wrong signature is rejected and logged at ERROR.
4. A missing bundle leaves the previous policy set in place.
5. Every decision is recorded in an audit table.

---

## 5. Milestone 16 — User-Mode Enforcement

**Goal:** Enforce `Block` decisions for files, clipboard, and print jobs without kernel support.

### 5.1 Deliverables

- `IEnforcer` interface
- `FileEnforcer` — inotify + fanotify-based, blocks file writes
- `ClipboardEnforcer` — X11 / Wayland clipboard hooks (Linux), Windows clipboard (Windows)
- `PrintEnforcer` — CUPS hook (Linux), spooler hook (Windows)
- `EnforcerRegistry` — routes decisions to the right enforcer

### 5.2 Interface

```cpp
// src/enforcement/enforcer.h

class IEnforcer {
public:
    virtual ~IEnforcer() = default;
    virtual std::string name() const = 0;
    virtual bool can_enforce(const std::string& event_type) const = 0;
    virtual bool enforce(const Decision& decision, const Event& event) = 0;
};
```

### 5.3 FileEnforcer (Linux)

Uses **fanotify** in `FAN_OPEN_PERM` mode:

1. Open a fanotify fd with `FAN_CLASS_CONTENT`.
2. Mark the mount points we care about.
3. On `FAN_OPEN_PERM` event, suspend the calling process.
4. Publish a `file_access` event, wait for a `Decision`.
5. Call `write(fd, response)` with `FAN_ALLOW` or `FAN_DENY`.

Limitation: fanotify permission events require `CAP_SYS_ADMIN`. The agent must run as a service (root on Linux).

**Fallback:** if fanotify is unavailable, use inotify + `LD_PRELOAD` shim or accept that writes complete but are then quarantined.

### 5.4 FileEnforcer (Windows)

Windows lacks a user-mode API to block file opens. Two options:

1. **Minifilter** (Milestone 21) — kernel-level, unbypassable.
2. **Read-only folder trick** — temporarily mark the target directory read-only when a block is decided. Racy and bypassable.

For Phase 2 we implement a **notification-only** FileEnforcer on Windows and defer blocking to Milestone 21.

### 5.5 ClipboardEnforcer (Linux)

- X11: own `CLIPBOARD` and `PRIMARY` selections; intercept `XConvertSelection` requests.
- Wayland: use `wlr-data-control-unstable-v1` protocol (Wayland compositor support varies).

The enforcer scans every clipboard payload:
- If `Decision::Block`, do not serve the selection.
- Otherwise serve it.

### 5.6 ClipboardEnforcer (Windows)

- Register a clipboard viewer with `AddClipboardFormatListener`.
- On `WM_CLIPBOARDUPDATE`, read the clipboard, scan it.
- If `Block`, call `EmptyClipboard()` and show a notification.

### 5.7 PrintEnforcer (Linux)

- Watch `/var/spool/cups/tmp/` for new spool files.
- On creation, open and scan.
- If `Block`, delete the spool file and send a CUPS cancel.

### 5.8 PrintEnforcer (Windows)

- Register a print monitor with `AddMonitor` API.
- Intercept `StartDocPrinter`, `WritePrinter`.
- Scan the spooled data before it reaches the device.

### 5.9 Success Criteria

1. A `Block` decision on `file_access` prevents the read on Linux.
2. A `Block` decision on `clipboard_copy` clears the clipboard within 100 ms.
3. A `Block` decision on `print_job` cancels the print before it leaves the spooler.
4. Every enforcement action is recorded in the audit log with the policy ID.
5. Enforcement failure does not crash the agent — it logs and continues.

---

## 6. Milestone 17 — Extended Content Extraction

**Goal:** Scan PDF, DOCX, XLSX, PPTX, and images with OCR.

### 6.1 Deliverables

- `IContentExtractor` interface
- `PdfExtractor` — Poppler or MuPDF
- `DocxExtractor` — libzip + XML parsing
- `XlsxExtractor` — same
- `PptxExtractor` — same
- `ImageOcrExtractor` — PaddleOCR C++ bindings (ncnn)
- `ExtractorRegistry` — routes file types to the right extractor
- Content-type detection via magic bytes (libmagic)

### 6.2 Interface

```cpp
// src/extractors/extractor.h

struct ExtractedContent {
    std::string text;
    std::vector<std::string> images_b64;   // embedded images for OCR
    std::map<std::string, std::string> metadata;
    std::string error;
    bool ok() const { return error.empty(); }
};

class IContentExtractor {
public:
    virtual ~IContentExtractor() = default;
    virtual std::string name() const = 0;
    virtual bool supports(const std::string& mime, const std::string& ext) const = 0;
    virtual ExtractedContent extract(const std::vector<uint8_t>& data,
                                     const std::string& filename) = 0;
};
```

### 6.3 Dependencies

| Extractor | Library | License | Notes |
|-----------|---------|---------|-------|
| PDF | Poppler (libpoppler-cpp) | GPL-2.0 | Add a note about GPL linkage |
| PDF (alt) | MuPDF | AGPL | Not compatible with commercial agent |
| DOCX/XLSX/PPTX | libzip + pugixml | BSD/MIT | — |
| OCR | PaddleOCR + ncnn | Apache 2.0 | Bundle as static library |
| Magic | libmagic | BSD | — |

Choose **Poppler** for PDF unless licensing forbids GPL; then fall back to a commercial SDK (PDFium via Chromium build).

### 6.4 Pipeline

```
content_scanner (existing)
    |
    v
ExtractorRegistry::extract(data, filename) 
    |
    +-- by magic bytes → PDF   → PdfExtractor
    +-- by magic bytes → ZIP   → try DOCX/XLSX/PPTX
    +-- by magic bytes → image → ImageOcrExtractor
    +-- else            → treat as text
    |
    v
ExtractedContent { text, images_b64 }
    |
    +-- text → chunk + send to server
    +-- images → OCR each → append text → chunk + send
```

### 6.5 Success Criteria

1. A 50-page PDF extracts to text in under 3 seconds.
2. A DOCX with embedded images extracts text + OCRs each image.
3. An XLSX with 10 sheets extracts all cells including formulas.
4. A PNG or JPG containing text produces the text via OCR.
5. Extraction errors do not crash the agent.

---

## 7. Milestone 18 — Advanced Detection

**Goal:** Move beyond regex — exact data match, document fingerprinting, and structured data.

### 7.1 Deliverables

- `EdmMatcher` — Exact Data Match (columns of known-sensitive values)
- `PdmMatcher` — Partial Document Match (shingled fingerprints)
- `DocumentRegistry` — a signed corpus of sensitive documents
- `StructuredDataMatcher` — CSV/JSON records against a schema
- `FingerprintStore` — SQLite-backed

### 7.2 EDM

- The server compiles a list of exact values (SSNs, card numbers, patient IDs) into a Bloom filter + HMAC.
- The filter + HMAC key are pushed to the agent via the policy bundle.
- On scan, the agent HMACs every token in the file and checks membership in the filter.
- No plaintext values on the endpoint — only HMACs of them.

```cpp
class EdmMatcher {
public:
    bool match(const std::string& text,
               std::vector<std::string>& out_matches) const;
private:
    // Bloom filter of HMAC(value, key)
    std::vector<uint8_t> filter_;
    std::array<uint8_t, 32> hmac_key_;
    // Number of hash functions, filter size
    uint32_t k_ = 7;
    uint64_t m_ = 0;
};
```

### 7.3 PDM

- The server shingles each protected document into 30-character chunks with stride 10.
- Each shingle is hashed and stored in a Bloom filter.
- The agent computes the same shingles on a candidate file and counts matching shingles.
- Above a threshold (e.g., 25% of shingles match), the file is flagged.

### 7.4 DocumentRegistry

- Signed list of document IDs with metadata (name, category, sensitivity).
- The registry is downloaded and cached.
- The matcher returns the document ID + name when a match is found.

### 7.5 Structured Data

- Policies can embed a schema (e.g., "row must contain both credit_card AND email").
- The matcher parses CSV/JSON and applies the rules row by row.

### 7.6 Success Criteria

1. A file containing one exact SSN from the EDM list is flagged.
2. A file with 25% PDM shingle match is flagged with the source document name.
3. A CSV with one row meeting a structured-data rule is flagged.
4. EDM list of 10 million values fits in under 20 MB of RAM.
5. Match time for a 1 MB file is under 50 ms.

---

## 8. Milestone 19 — IPC & Tray Helper

**Goal:** Give the user a visible agent, notifications, and pause/resume.

### 8.1 Deliverables

- `IPCServer` in the service (Unix socket on Linux, named pipe on Windows)
- `axiomdlp-helper` binary — tray icon + notifications + IPC client
- Message protocol (length-prefixed JSON)
- Auto-start of helper on user login (Linux: XDG autostart; Windows: Run key)

### 8.2 IPC Protocol

Message shape:

```json
{
  "type": "notification",
  "id":   "evt_abc123",
  "title": "Sensitive file blocked",
  "body":  "/home/user/Downloads/pii.txt contains credit card data",
  "actions": ["view_details", "dismiss"],
  "severity": "warning"
}
```

Types:
- `notification`
- `pause_dlp` / `resume_dlp`
- `query_status`
- `override_request` / `override_response`
- `user_feedback`
- `ping` / `pong`

### 8.3 Tray Icon

- **Linux:** libappindicator3 (`libayatana-appindicator3-dev`)
- **Windows:** `Shell_NotifyIcon` via Win32 API
- **States:** green (running), yellow (paused), red (error)
- **Menu:** Status, Pause for 15m, Pause until restart, View events, About, Quit

### 8.4 Notifications

- **Linux:** `notify-send` via D-Bus or `libnotify`
- **Windows:** `Shell_NotifyIcon` balloon or Windows toast (WinRT)
- Severity levels map to icon + color

### 8.5 Pause/Resume

- Helper sends `pause_dlp` with a TTL (seconds).
- Service sets a `paused_until` timestamp.
- Enforcer registry checks the timestamp before enforcing.
- Policy can disallow pause (e.g., in "strict" mode).

### 8.6 Override Request

When a user is blocked, they can click "Request override":

1. Helper opens a dialog for justification.
2. Sends `override_request` over IPC.
3. Service publishes an event; the Core receives it over WebSocket.
4. The admin approves/denies on the server.
5. The response flows back through the WebSocket → IPC → helper notification.

### 8.7 Success Criteria

1. Tray icon appears when the user logs in.
2. A blocked event shows a notification within 200 ms.
3. Pause/resume takes effect within 1 s.
4. Helper crash does not affect the service.
5. Service restart reconnects the helper automatically.

---

## 9. Milestone 20 — Kernel Enforcement (Linux eBPF)

**Goal:** Unbypassable enforcement of file operations via eBPF LSM programs.

### 9.1 Deliverables

- eBPF LSM programs: `file_open`, `file_permission`, `task_kill`
- eBPF map for kernel↔user communication
- User-space loader (libbpf)
- BPF CO-RE (Compile Once – Run Everywhere)

### 9.2 Architecture

```
+----------------------+          +---------------------+
|   User processes     |          |                     |
|   open(), exec()     | ----->   |  Kernel LSM hooks   |
+----------------------+          |    (BPF programs)   |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  BPF ring buffer    |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  User-space loader  |
                                  |   (agent process)   |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |   PolicyEngine      |
                                  +---------------------+
```

### 9.3 Program 1 — `file_open`

Triggered on `security_file_open`. Looks up the file path in a BPF hash map. If present and marked DENY, returns `-EACCES`.

The map is populated by user-space when the policy engine decides to block a specific path.

### 9.4 Program 2 — `file_permission`

Triggered on `security_file_permission` for write operations. Same as above but for writes.

### 9.5 Program 3 — `task_kill`

Triggered on `sched_process_exit`. Used for telemetry, not enforcement.

### 9.6 Communication Channel

- User-space writes decisions to a BPF hash map keyed by `(dev, inode)`.
- Kernel program looks up by `(dev, inode)`.
- Value: `u8` (0 = allow, 1 = deny) + TTL.

### 9.7 Loading

- `libbpf` (with CO-RE) + a `.bpf.o` object compiled at build time.
- Attached via `bpf_link` to the LSM hook.
- Unloaded on agent shutdown.

### 9.8 Requirements

- Linux 5.7+ (BPF LSM)
- `CONFIG_BPF_LSM=y` in the running kernel
- The agent must run as root or with `CAP_BPF` + `CAP_PERFMON`

### 9.9 Fallback

If BPF LSM is unavailable, fall back to fanotify (Milestone 16).

### 9.10 Success Criteria

1. A blocked file open returns `EACCES` to the caller.
2. `strace -f /usr/bin/cat blocked.txt` shows `EACCES`.
3. Direct syscalls (`syscall(SYS_open, ...)`) also return `EACCES`.
4. No kernel oops under load.
5. Agent unload removes all BPF programs cleanly.

---

## 10. Milestone 21 — Kernel Enforcement (Windows Minifilter)

**Goal:** Unbypassable enforcement of file operations on Windows.

### 10.1 Deliverables

- Minifilter driver (`axiomdlpflt.sys`)
- User-space communication library
- Installer / uninstaller
- Driver signing (test cert for dev, EV cert for production)

### 10.2 Architecture

```
+----------------------+          +---------------------+
|  Win32 file APIs     |          |                     |
|  CreateFile()        | ----->   |  Windows I/O Mgr    |
+----------------------+          +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  Filter Manager     |
                                  |   (fltmgr.sys)      |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  axiomdlpflt.sys    |
                                  +----------+----------+
                                             |
                                             v
                                  +---------------------+
                                  |  Agent (user-mode)  |
                                  |  via FltSendMessage |
                                  +---------------------+
```

### 10.3 Driver Responsibilities

- Register `IRP_MJ_CREATE` pre-op callback
- On each file open, send a message to user-space
- Wait up to 5 seconds for a response
- Response: `FLT_PREOP_SUCCESS_NO_CALLBACK` (allow) or `FLT_PREOP_COMPLETE` with `STATUS_ACCESS_DENIED` (block)

### 10.4 User-Space Communicator

- Uses `FilterConnectCommunicationPort` + `FilterSendMessage` + `FilterGetMessage`
- Runs on a dedicated thread
- Calls `PolicyEngine::evaluate` and returns the decision

### 10.5 Installation

- `axiomdlpflt.inf` — the driver INF
- `sc create axiomdlpflt binPath= ... type= kernel`
- `fltmc load axiomdlpflt`
- Uninstall: `fltmc unload` + `sc delete`

### 10.6 Requirements

- Windows 10+ (driver model 1.9+)
- Driver signed with a Microsoft‑approved cert (EV or test with `bcdedit /set testsigning on` in dev)

### 10.7 Success Criteria

1. `CreateFile()` on a blocked path returns `ERROR_ACCESS_DENIED`.
2. Direct NT API calls (`NtCreateFile`) also blocked.
3. Driver unloads cleanly without a bugcheck.
4. No file system corruption under stress.
5. Signed driver loads on Windows 10 and 11 x64.

---

## 11. Milestone 22 — Remote Commands & Auto-Update

**Goal:** Control the fleet from the Core and update agents without admin intervention.

### 11.1 Deliverables

- Command dispatcher on the agent (subscribe to `command` WebSocket messages)
- Signature verification for update bundles
- Atomic binary swap on Linux and Windows
- Rollback on failed startup

### 11.2 Commands

| Command | Payload | Action |
|---------|---------|--------|
| `rescan` | `{}` | Trigger a full inventory scan |
| `pause` | `{"duration_sec": 900}` | Pause enforcement |
| `resume` | `{}` | Resume enforcement |
| `quarantine_file` | `{"path": "/..."}` | Move to vault |
| `uninstall` | `{}` | Uninstall and exit |
| `update` | `{"url": "...", "sha256": "...", "sig": "..."}` | Download, verify, install |
| `restart` | `{}` | Restart the service |
| `dump_logs` | `{"lines": 1000}` | Upload recent logs |
| `run_diagnostic` | `{}` | Run self-test, upload results |

### 11.3 Update Flow

1. Agent receives `update` command with URL, SHA-256, and Ed25519 signature.
2. Downloads to `/var/lib/axiomdlp/updates/axiomdlp-agent.new`.
3. Verifies SHA-256.
4. Verifies Ed25519 signature against the embedded public key.
5. On Linux:
   - Rename current binary to `.old`
   - Rename `.new` to current path
   - `chmod +x`
   - Restart via systemd (`systemctl restart axiomdlp-agent`)
   - On startup, if the new binary fails a self-test within 30 seconds, revert to `.old`
6. On Windows:
   - Schedule the swap via `MoveFileEx(MOVEFILE_DELAY_UNTIL_REBOOT)` OR
   - Use a helper service running outside the agent
   - Verify by re-launch and self-test

### 11.4 Embedded Public Key

```cpp
// src/updater/trusted_keys.h
constexpr const char* kUpdateSigningKeyHex =
    "9f8e7d6c5b4a39281706f5e4d3c2b1a0..."; // 32-byte Ed25519 public key
```

The key is compiled into the binary and cannot be replaced without a signed update.

### 11.5 Success Criteria

1. An unsigned update is rejected and logged at ERROR.
2. A signed update installs and the agent restarts within 10 seconds.
3. A corrupted binary fails self-test and rolls back automatically.
4. Linux update works without a restart via `systemd-run`.

---

## 12. Milestone 23 — Platform Expansion

**Goal:** macOS and ARM64 support.

### 12.1 macOS

- **Filesystem watching:** FSEvents API
- **Enforcement:** EndpointSecurity framework (requires Apple entitlements)
- **Service:** launchd plist
- **Tray:** NSStatusBar
- **Signing:** Apple Developer ID + notarization
- **Distribution:** `.pkg` signed and notarized

### 12.2 ARM64 Linux

- Add `cmake/toolchains/linux-aarch64.cmake`
- Cross-compile via `aarch64-linux-gnu-g++`
- Build OpenSSL for aarch64
- Verify eBPF works on aarch64

### 12.3 Windows ARM64

- Add `windows-aarch64.cmake` via llvm-mingw
- Build OpenSSL with `VC-WIN64-ARM`
- Verify minifilter loads on Surface Pro X

### 12.4 Success Criteria

1. All six targets build on CI: linux‑x86_64, linux‑i686, linux‑aarch64, windows‑x86_64, windows‑i686, windows‑aarch64.
2. macOS agent starts, watches files, and enforces blocks.
3. Binary sizes remain under 15 MB compressed.
4. ARM64 binaries run on Raspberry Pi 4 (Linux) and Surface Pro X (Windows).

---

## 13. Milestone 24 — Observability & Telemetry

**Goal:** Every subsystem exposes metrics for fleet-wide visibility.

### 13.1 Deliverables

- `MetricsRegistry` — process-wide metrics
- HTTP endpoint on `127.0.0.1:9101/metrics` in Prometheus text format
- Structured logs with correlation IDs
- Optional OpenTelemetry exporter

### 13.2 Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `axiom_agent_uptime_seconds` | gauge | Process uptime |
| `axiom_agent_module_health` | gauge | Per-module health 0/1/2/3 |
| `axiom_events_published_total` | counter | Events on the bus |
| `axiom_events_dropped_total` | counter | Outbound queue overflows |
| `axiom_scan_files_total` | counter | Files scanned |
| `axiom_scan_bytes_total` | counter | Bytes scanned |
| `axiom_scan_duration_seconds` | histogram | Scan latency |
| `axiom_decisions_total{action}` | counter | Decisions by action |
| `axiom_enforcement_total{result}` | counter | Enforcement by outcome |
| `axiom_policy_version` | gauge | Current policy version |
| `axiom_ws_reconnects_total` | counter | WebSocket reconnect attempts |
| `axiom_ws_connected` | gauge | 1 if connected |
| `axiom_ipc_connections` | gauge | Active helper connections |

### 13.3 Log Correlation

Every event carries a `trace_id` (ULID). Logs include `trace_id` in the structured prefix so a single event can be followed from filesystem watcher to server decision to enforcement action.

### 13.4 OpenTelemetry (optional)

If `OTEL_EXPORTER_OTLP_ENDPOINT` is set, spans are exported:
- `content_scan` (span per file)
- `policy_evaluate` (span per decision)
- `enforce` (span per enforcement action)
- `ws_publish` (span per outbound message)

### 13.5 Success Criteria

1. `/metrics` returns valid Prometheus text.
2. Grafana scrapes and shows a full dashboard.
3. A single event can be traced end-to-end across the bus.
4. Metrics add < 1% CPU overhead.

---

## 14. Milestone 25 — Hardening & Tamper Resistance

**Goal:** The agent cannot be disabled, killed, or bypassed by a non-admin user.

### 14.1 Deliverables

- Service watchdog (a second process that restarts the agent if it dies)
- Anti-kill (kernel-level process protection via eBPF / minifilter)
- Configuration integrity check (HMAC over config file)
- Binary integrity check (self-hash compared to a compiled-in value)
- Signed configuration and policy bundles (already covered)
- Protected directories: `logs/`, `data/`, `updates/` are chmod 0700 and owned by root

### 14.2 Watchdog

Two processes:
- `axiomdlp-agent` — main
- `axiomdlp-watchdog` — small, monitors the main via a heartbeat file

If the heartbeat file is not touched within 30 seconds, the watchdog restarts the agent via systemd / SCM.

The watchdog itself is protected by:
- `ProtectSystem=strict` in the systemd unit
- Runs as a separate systemd unit that the main cannot stop (no `systemctl stop` permission)

### 14.3 Anti-Kill (Linux)

eBPF LSM `task_kill` program: if the target is the agent's process, deny the kill with `-EPERM`. Requires `CAP_BPF` and a kernel with BPF LSM.

### 14.4 Anti-Kill (Windows)

Minifilter pre-op on `IRP_MJ_SET_INFORMATION` with `FileDispositionInformation` for the agent's own binary and data files. Returns `STATUS_ACCESS_DENIED` if a non-system process tries to delete them.

A more robust approach uses an ObRegisterCallbacks hook to strip `PROCESS_TERMINATE` from handles opened against the agent's PID.

### 14.5 Configuration Integrity

On startup:
1. Read `config/agent.json`.
2. Compute HMAC-SHA256 with a key derived from a machine-specific secret (TPM or file ACL).
3. Compare against `config/agent.json.hmac`.
4. If mismatch, log ERROR and load defaults.

### 14.6 Binary Integrity

Embed a SHA-256 of the running binary at build time. On startup:
1. Reopen `/proc/self/exe` (Linux) or the module path (Windows).
2. Compute SHA-256.
3. Compare against the embedded value.
4. If mismatch, log ERROR and continue in "degraded" mode with limited enforcement.

### 14.7 Success Criteria

1. `kill -9` on the agent as a non-root user fails with `EPERM`.
2. Deleting the agent binary as a non-root user fails.
3. Modifying the config file is detected at next startup.
4. Watchdog restarts the agent within 30 seconds of a crash.
5. None of the above prevents legitimate admin operations.

---

## 15. Testing Strategy

### 15.1 Unit Tests

- **C++:** Catch2 + doctest + GoogleTest
- Target: 80% coverage on `src/policy`, `src/enforcement`, `src/extractors`, `src/fingerprint`
- Run on every commit via CI

### 15.2 Integration Tests

- **Agent ↔ Server:** spin up the Go server on a test port, run the agent in a container, assert end-to-end behavior
- Use a dedicated test database
- Cover: enrollment, heartbeat, WebSocket connect, policy update, scan batch, remote command

### 15.3 Load Tests

- 10,000 simulated agents connect to the server
- Sustained 100 req/s per agent for 10 minutes
- Assert no server-side leaks, no dropped messages

### 15.4 Chaos Tests

- Kill the server mid-scan → agent must not lose data
- Network partition → agent must queue and recover
- Corrupt policy file → agent must reject and continue with old policy
- Fill the disk → agent must not crash

### 15.5 Fuzz Tests

- **PDF extractor:** libFuzzer with a seed corpus
- **DOCX extractor:** same
- **WebSocket parser:** AFL
- **IPC protocol:** libFuzzer

### 15.6 Penetration Tests

Every release:
- Attempt to bypass enforcement
- Attempt to forge a policy update
- Attempt to kill the agent
- Attempt to read agent's private state

### 15.7 Certification

- SOC 2 Type II
- ISO 27001
- Common Criteria (EAL2+)
- FIPS 140-2 for crypto modules

---

## 16. Build & Release

### 16.1 CI Pipeline

Triggered on every push:

1. Lint (clang-tidy, clang-format, cppcheck)
2. Unit tests on linux‑x86_64
3. Cross-compile for linux‑i686, windows‑x86_64, windows‑i686
4. Integration tests against a mock server
5. SBOM generation (CycloneDX)
6. Upload artifacts

Triggered on every tag:

1. All of the above
2. Sign binaries with the release key
3. Build `.deb`, `.rpm`, `.msi`
4. Sign packages
5. Publish to the release page
6. Publish SBOM

### 16.2 Versioning

- Semantic versioning: `MAJOR.MINOR.PATCH`
- Every binary embeds the git commit SHA
- Every update bundle includes a `min_version` field

### 16.3 Distribution

- **Linux:** `.deb` + `.rpm` + a static tarball
- **Windows:** `.msi` + a static `.exe`
- **macOS:** signed `.pkg`
- **Docker:** `axiomdlp/agent:<tag>` for testing

---

## 17. Working Agreement

### 17.1 Per Milestone

1. Design doc in this README
2. Interface in header files
3. Reference implementation
4. Unit tests
5. Integration tests
6. All four targets build clean
7. Commit with a descriptive message
8. Update this README to reflect the new state

### 17.2 Every New Module Must

- Implement `IModule`
- Receive `ModuleContext`
- Publish or subscribe to events via the bus
- Report health
- Have a metrics section
- Be fail-safe (errors logged, not fatal)
- Be documented in this README

### 17.3 Every New Subsystem Must

- Follow the three-file pattern: header, implementation, CMake wiring
- Have platform-specific implementations in `platform/<os>/` if it touches the OS
- Have a debug flag to disable it at runtime
- Have a self-test that runs at startup

### 17.4 Every Release Must

- Pass all unit + integration tests
- Have a signed SBOM
- Have a changelog entry
- Be backward compatible with the previous MINOR version's config

### 17.5 Definition of Done

- [ ] Code committed
- [ ] Tests pass
- [ ] Docs updated
- [ ] This README updated
- [ ] All six targets build (once Phase 2 is complete)
- [ ] Metrics exported
- [ ] Logs structured
- [ ] Error paths exercised

---

## Summary

**Phase 2 deliverables in order:**

| # | Milestone | Depends On |
|---|-----------|------------|
| 15 | Policy Engine | — |
| 16 | User-Mode Enforcement | 15 |
| 17 | Extended Extraction | — |
| 18 | Advanced Detection | 17 |
| 19 | IPC & Tray Helper | 16 |
| 20 | Kernel Enforcement (Linux) | 16 |
| 21 | Kernel Enforcement (Windows) | 16 |
| 22 | Remote Commands & Auto-Update | 19 |
| 23 | Platform Expansion | 22 |
| 24 | Observability | 15 |
| 25 | Hardening | 20, 21, 22 |

**Estimated effort:** 6–9 months for one experienced systems engineer, 3–4 months for a small team of 3.

**After Phase 2, the agent will be a production-grade enterprise DLP endpoint** on par with endpoint DLP features found in Forcepoint, Symantec, and Microsoft Defender for Endpoint.