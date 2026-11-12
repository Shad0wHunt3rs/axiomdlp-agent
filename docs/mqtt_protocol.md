# `docs/mqtt_protocol.md`

Full document — commit this byte-identically at both `axiomdlp-core/docs/mqtt_protocol.md` and `axiomdlp-agent/docs/mqtt_protocol.md`. Any change here is a **two-repo** change and requires a coordinated commit.

---

```markdown
# AxiomDLP Agent ↔ Core — MQTT Wire Protocol

**Version:** 1 (Phase 0 lock-in)
**Status:** Active. Replaces gRPC (`axiomdlp.agent.v1`).
**Owners:** Both `axiomdlp-core` and `axiomdlp-agent`.
**Rule:** This document MUST be byte-identical in both repositories.
Any change requires a coordinated commit in both and a version bump
(section 13).

---

## Table of contents

1.  [Protocol summary](#1-protocol-summary)
2.  [Transport layer](#2-transport-layer)
3.  [Client identity and authentication](#3-client-identity-and-authentication)
4.  [Topics](#4-topics)
5.  [QoS matrix](#5-qos-matrix)
6.  [Request / response correlation](#6-request--response-correlation)
7.  [Error model](#7-error-model)
8.  [Last Will & Testament](#8-last-will--testament)
9.  [Message schemas](#9-message-schemas)
10. [End-to-end sequences](#10-end-to-end-sequences)
11. [Migration mapping (gRPC → MQTT)](#11-migration-mapping-grpc--mqtt)
12. [Deferred items (deliberate non-goals)](#12-deferred-items-deliberate-non-goals)
13. [Versioning](#13-versioning)
14. [Reference implementation](#14-reference-implementation)
15. [Verification checklist](#15-verification-checklist)
16. [Security considerations](#16-security-considerations)

---

## 1. Protocol summary

| Aspect | Decision |
|---|---|
| Transport protocol | **MQTT 5.0** (OASIS standard) |
| Serialization | **UTF-8 JSON** |
| Broker (server side) | **Mochi-MQTT v2**, embedded in the Go Core process |
| Client library (agent side) | **libmosquitto** (C), MQTT 5 mode |
| Default port | **8883** (TLS), **1883** (plaintext, dev only) |
| Binary payloads | Base64 inside JSON (`*_b64` fields) |
| Version field | `"v": 1` **required** on every message |
| Topic root | `axiomdlp/` |
| Wildcards | Only server-side; agents MUST NOT subscribe with `+` or `#` under `axiomdlp/agents/` or `axiomdlp/replies/` |

**Rejection rule:** any message with `v != 1` MUST be rejected by the
receiver with a response payload `{"ok": false, "error": "unsupported_version"}`.

**Why MQTT and not gRPC:** see the migration record in
`done.md` (§Phase 6). Short version: gRPC's `protoc` codegen, imported
CMake target path pollution, and static Brotli/OpenSSL linking
requirements made 32-bit cross-compilation unreliable across toolchain
upgrades. MQTT has none of these properties. It is a proven pattern for
endpoint security agents (CrowdStrike, Trellix, OpenText, ESET all use
broker-mediated protocols for the same use case).

---

## 2. Transport layer

### 2.1 TLS

| Item | Value | Notes |
|---|---|---|
| TLS | **Required in production** | Same OpenSSL 3.3.2 as the HTTP path |
| Minimum TLS version | **1.2** | TLS 1.0/1.1 rejected by both sides |
| SNI | Client **MUST** send SNI | Same as the HTTP path |
| Certificate verification | **Required**, no opt-out | `mqtt_ca_path` overrides system roots; empty = system roots |
| Cipher suites | OpenSSL 3.3.2 defaults | No additional restrictions |
| Certificate pins | Not used in Phase 1 | Reserved for a future hardening milestone |

### 2.2 Connection parameters

| Parameter | Value | Set by | Notes |
|---|---|---|---|
| Keep-alive interval | **60 s** | Client (`mosquitto_connect_async`) | Broker disconnects clients that do not PINGREQ within 1.5 × keep-alive |
| Clean Start | **`false`** | Client | Persistent session; broker retains state |
| Session Expiry Interval | **3600 s** (1 hour) | Client (MQTT 5 CONNECT property) | Broker retains subscriptions and QoS 1/2 queued messages for 1 h after disconnect |
| Receive Maximum | **100** | Both sides | MQTT 5 flow control |
| Maximum Packet Size | **134217728 bytes** (128 MB) | Both sides | Matches the old gRPC `MaxRecvMsgSize` |
| Topic Alias Maximum | **0** (disabled) | Both sides | Simplest configuration; agents send full topics |
| Request Response Information | `1` | Server (CONNACK) | Tells the client the broker supports MQTT 5 request/response |
| Retain Available | `1` | Server (CONNACK) | Server→agent control frames use retained messages |
| Shared Subscriptions Available | `0` | Server (CONNACK) | Deferred (see §12) |

### 2.3 Reconnect behavior

The agent's MQTT transport module MUST reconnect with:

- **Exponential backoff** with 20% jitter.
- **Base delay:** `server.reconnect_backoff_seconds` (default 10 s).
- **Max delay:** `server.reconnect_backoff_max_seconds` (default 300 s).
- **No backoff reset until** a `CONNACK` with `Reason Code = 0x00 (Success)`
  AND at least one successful round-trip has occurred.

On reconnect, the agent MUST:

1. Re-subscribe to `axiomdlp/replies/{instance_uuid}` (clean start is `false`, but the server-side subscription set can change if the session expired).
2. Re-subscribe to `axiomdlp/agents/{agent_id}/command`, `/policy`, `/revoke` **only if** `agent_id` is known.
3. Reconcile `agent_id` from the local `state` table; if it is absent or if any subsequent request returns `{"ok":false,"error":"not_found"}`, re-register via `axiomdlp/register`.

### 2.4 Session persistence

Because Clean Start is `false`:

- The broker retains the agent's subscriptions across disconnects (for up to Session Expiry Interval = 1 h).
- Queued QoS 1/2 messages are delivered on reconnect.
- **Retained** messages on `axiomdlp/agents/{agent_id}/policy` and `axiomdlp/agents/{agent_id}/revoke` are re-delivered on every subscribe — this is intentional (see §4.3).

The agent MUST NOT depend on the broker retaining its session beyond the 1-hour window. On any reconnect, it re-establishes its subscriptions defensively.

---

## 3. Client identity and authentication

### 3.1 MQTT CONNECT fields

| MQTT 5 field | Value | Notes |
|---|---|---|
| Client ID | `axiomdlp-{instance_uuid}` | `instance_uuid` is a ULID (26 chars), generated on first launch, persisted in the agent's local SQLite `state` table under key `mqtt_instance_uuid` |
| Username | The plaintext `axdlp_user_...` API token | The token issued by `POST /api/v1/me/tokens` or `POST /api/v1/register` |
| Password | **Empty string** | MQTT 5 allows empty password; the token is the only credential |
| Clean Start | `false` | See §2.2 |
| Session Expiry Interval | `3600` | See §2.2 |
| Keep Alive | `60` | See §2.2 |

### 3.2 Server authentication flow

The Core's `AuthHook.OnConnectAuthenticate` hook (see
`internal/mqtt/auth.go`) performs, in order:

1. Extract `Username` from the CONNECT packet.
2. Reject immediately (log `empty username`) if it is empty.
3. Compute `SHA-256(Username)` — this is the same hash the HTTP agent middleware uses, so a token that works over HTTP works identically over MQTT.
4. Look up `user_api_tokens` row via `GetByHashSuper(ctx, hash)`.
   - **Not found, or revoked (`revoked_at IS NOT NULL`)** → reject with CONNACK `0x87 (Not authorized)`.
   - **DB error** → fail closed, reject with CONNACK `0x87`.
5. Record the resolved identity in an in-process side map keyed by MQTT Client ID:
   ```
   { user_id, org_id, token_id }
   ```
   (mochi-mqtt v2's `Client` struct has no user metadata field, so the hook owns the map — see `internal/mqtt/auth.go`.)
6. Best-effort: update `user_api_tokens.last_used_at` under a super-admin transaction.
7. Return `true` → CONNACK `0x00 (Success)`.

### 3.3 Invalid token handling

When `auth` fails:

- The broker sends `CONNACK 0x87 (Not authorized)` and closes the TCP connection.
- The agent MUST treat `0x87` as a **permanent failure** — no retry. It is equivalent to the old gRPC `UNAUTHENTICATED` and MUST trigger `ShutdownController::request(AuthRevoked)`.
- The agent logs `mqtt auth: unknown or revoked token` on its own side with the token prefix (first 20 characters).

### 3.4 Token rotation

Token rotation is not supported in Phase 1. To rotate:

1. Issue a new token via `POST /api/v1/me/tokens`.
2. Update the agent's config `server.api_token`.
3. Restart the agent.

Live rotation (without restart) is deferred to a future phase.

### 3.5 Agent ID

The MQTT session does **not** carry `agent_id` at CONNECT time. `agent_id` is assigned by the server in the `Register` response, persisted locally by the agent, and used thereafter only as a topic segment:

```
axiomdlp/agents/{agent_id}/heartbeat
axiomdlp/agents/{agent_id}/scan/batch
...
```

The server verifies on every message that the `agent_id` segment matches an agent owned by the authenticated `user_id`. A mismatch is rejected with `{"ok": false, "error": "not_found"}` — indistinguishable from an unknown agent, to prevent enumeration.

---

## 4. Topics

Topic root is `axiomdlp/`. Four topic families exist:

```
axiomdlp/register                              (global, one instance)
axiomdlp/agents/{agent_id}/*                   (per-agent request/control)
axiomdlp/replies/{instance_uuid}               (per-agent-instance responses)
axiomdlp/sessions/{instance_uuid}/*            (per-agent-instance presence)
```

### 4.1 Agent → Server (requests)

| Topic | Direction | QoS | Retain | Response? |
|---|---|---|---|---|
| `axiomdlp/register` | Agent → Server | 1 | No | Yes (via `Response-Topic`) |
| `axiomdlp/agents/{agent_id}/heartbeat` | Agent → Server | 1 | No | Yes (via `Response-Topic`) |
| `axiomdlp/agents/{agent_id}/scan/batch` | Agent → Server | 1 | No | Yes (via `Response-Topic`) |
| `axiomdlp/agents/{agent_id}/shutdown` | Agent → Server | 1 | No | **No** |

**Server subscription patterns:**

```
axiomdlp/register
axiomdlp/agents/+/heartbeat
axiomdlp/agents/+/scan/batch
axiomdlp/agents/+/shutdown
```

### 4.2 Server → Agent (responses)

| Topic | Direction | QoS |
|---|---|---|
| `axiomdlp/replies/{instance_uuid}` | Server → Agent | Same QoS as the request (1) |

The `{instance_uuid}` comes from the request's `Response-Topic` MQTT 5 property. The agent subscribes to its own reply topic **once** on connect and routes incoming responses by the `Correlation-Data` property (see §6).

### 4.3 Server → Agent (server-initiated)

| Topic | Direction | QoS | Retain | Purpose |
|---|---|---|---|---|
| `axiomdlp/agents/{agent_id}/command` | Server → Agent | **2** | **No** | `ServerCommand` — imperative (rescan, pause, resume, dump_logs, run_diagnostic, ...) |
| `axiomdlp/agents/{agent_id}/policy` | Server → Agent | **2** | **Yes** | `ServerPolicyUpdate` — signed policy bundle |
| `axiomdlp/agents/{agent_id}/revoke` | Server → Agent | **2** | **Yes** | `ServerRevoke` — session termination |

**Retain semantics:**

- **Policy is retained.** A reconnecting agent receives the latest policy bundle immediately on subscribe, without needing to request it. Retained policy means the broker always holds "the most recent value", which is exactly the semantics of policy state.
- **Revoke is retained.** A reconnecting agent that was revoked while disconnected receives the revoke frame on subscribe and shuts down immediately, before doing anything else. When the agent is un-revoked (administrative action), the server publishes an empty retained message to clear the retained state.

### 4.4 Presence

| Topic | Direction | QoS | Retain | Payload |
|---|---|---|---|---|
| `axiomdlp/sessions/{instance_uuid}/status` | Agent → Server | 1 | **No** | `{"status":"offline"}` (LWT only) |

The agent sets this as its **Last Will & Testament** at CONNECT time (see §8). If the agent disconnects ungracefully (network drop, crash, kill -9), the broker publishes the LWT payload on this topic after the Will Delay Interval. The server's `SessionHooks.OnDisconnect` fires regardless, so the LWT is a **secondary** signal used only for the SSE agent presence events, not for the AgentHub's authoritative connected set.

### 4.5 Wildcard subscription rules

**Agents MUST NOT** subscribe with `+` or `#` under:

- `axiomdlp/agents/`
- `axiomdlp/replies/`

The server rejects such SUBSCRIBE packets at `AuthHook.OnACLCheck` with a log line:

```
{"msg":"mqtt acl: wildcard subscription rejected","client_id":"...","topic":"..."}
```

This prevents a single token from subscribing to `axiomdlp/agents/+/scan/batch` and reading every other agent's scan traffic.

Wildcards under `axiomdlp/sessions/` are also blocked (agents do not need to know about other agents' presence).

### 4.6 Topic naming rules

- All topics are lowercase.
- Only `[a-z0-9/_-]` plus `{...}` placeholders in this document.
- No trailing slashes.
- `agent_id` is a positive decimal integer (matches `agents.id`).
- `instance_uuid` is a ULID (26 uppercase ASCII chars, Crockford base32).
- Do NOT put payload data (file paths, hostnames, entity types) in topics — that leaks information into broker logs and `$SYS` metrics.

---

## 5. QoS matrix

| Message | QoS | Rationale |
|---|---|---|
| `axiomdlp/register` request | **1** | At-least-once; duplicate registrations are absorbed by the server-side `(org, user, hostname)` deduplication |
| Register response | **1** | Client retries with a fresh correlation ID on timeout |
| Heartbeat request | **1** | Idempotent (`last_seen_at = now()`) |
| Heartbeat response | **1** | Idempotent |
| ScanBatch request | **1** | Server deduplicates by `(agent_id, file_hash, chunk_index)` in a 5-minute TTL cache |
| ScanBatch response | **1** | Client matches by correlation ID; duplicate responses are discarded |
| Shutdown | **1** | Best-effort; the broker also disconnects on close |
| `ServerCommand` | **2** | Non-idempotent — "rescan" fires once |
| `ServerPolicyUpdate` | **2** | Version monotonic, but ensure exactly-once delivery for signature verification cache |
| `ServerRevoke` | **2** | Critical: agent must shut down exactly once |
| LWT | **1** | Duplicate LWT messages are harmless; the server deduplicates on `instance_uuid` |

**Summary rule:** QoS 1 for volume (heartbeats, scans, registration); QoS 2 for control frames (command, policy, revoke). The 4-way handshake overhead of QoS 2 is paid only on low-frequency messages.

---

## 6. Request / response correlation

MQTT 5 provides native request/response correlation through two CONNECT-time and PUBLISH-time properties. The protocol uses both, plus a redundant JSON field for logging and debugging.

### 6.1 Three redundant signals on every request

**MQTT 5 `Response-Topic` property:**
```
axiomdlp/replies/{instance_uuid}
```
The server reads this on the incoming PUBLISH and uses it as the outgoing topic for the response.

**MQTT 5 `Correlation-Data` property:**
A ULID generated fresh for each request. Format: 26 ASCII uppercase characters (Crockford base32).

**JSON `correlation_id` field:**
Same ULID as the MQTT property. Redundant on the wire, but useful for logging and testing (grep a log by `correlation_id`).

### 6.2 Client-side bookkeeping

The agent's `MqttTransportModule` maintains:

```cpp
std::map<std::string /* correlation_id */,
         std::shared_ptr<PendingRequest>> pending_;

struct PendingRequest {
    std::condition_variable cv;
    std::optional<nlohmann::json> response;
    bool done = false;
};
```

Every `request(topic, payload, qos, timeout)` call:

1. Generates a ULID `correlation_id`.
2. Inserts a fresh `PendingRequest` keyed by that ID.
3. Publishes `payload` with `Response-Topic` and `Correlation-Data` properties set.
4. Blocks on `cv.wait_for(timeout)`.
5. When the reply arrives on the reply topic with matching `Correlation-Data`, the `on_message` callback fills `response` and signals `cv`.
6. Returns `response`, or `std::nullopt` on timeout.
7. **Always erases the entry from `pending_`**, whether the wait succeeded or timed out (RAII or explicit `pending_.erase(id)`).

### 6.3 Server-side handling

The message handler for the request reads:

1. `Correlation-Data` from the incoming PUBLISH properties.
2. `Response-Topic` from the incoming PUBLISH properties.

It builds the response payload (with the same `correlation_id` echoed in JSON), then publishes it to `Response-Topic` with `Correlation-Data` set to the same ULID. See `internal/mqtt/handlers_*.go`.

### 6.4 Timeouts and retries

**Client-side timeout:** `server.request_timeout_seconds` (default `300` s). On timeout:

- `request()` returns `std::nullopt`.
- The caller decides whether to retry.
- A retry uses a **fresh** `correlation_id` — the previous entry is already discarded.

**Server-side deduplication:**

- **Heartbeat:** idempotent, no dedup needed.
- **ScanBatch:** deduplicated by `(agent_id, file_hash, chunk_index)` with a 5-minute TTL in the server-side handler. A duplicate request produces the same result without re-running PII detection.
- **Register:** deduplicated by `(org_id, user_id, hostname)` in the repository — a duplicate register returns the same `agent_id` rather than creating a second row.

### 6.5 Concurrent requests

The agent MAY have multiple in-flight `request()` calls with distinct `correlation_id`s. The reply topic is shared, so the client must route each incoming reply by `Correlation-Data`; do NOT use the topic alone to route.

---

## 7. Error model

The protocol has two layers of error signaling: MQTT reason codes for **transport-level** failures, and a JSON error shape for **application-level** failures.

### 7.1 Application errors (in the JSON payload)

Every response message carries `ok: bool`. When `ok` is `false`, `error` is a machine-readable string. Both sides branch on `error`, never on any human-readable text (there is none).

| Situation | JSON payload | Notes |
|---|---|---|
| Success | `{"ok": true, ...actual response...}` | |
| Not found | `{"ok": false, "error": "not_found"}` | Unknown `agent_id` or cross-tenant access — indistinguishable, to prevent enumeration |
| Agent revoked | `{"ok": false, "error": "agent_revoked"}` | The agent's `status = 'revoked'`; the corresponding `ServerRevoke` frame is also published |
| Validation error | `{"ok": false, "error": "invalid_request", "details": {...}}` | `details` is an endpoint-specific object |
| Unsupported version | `{"ok": false, "error": "unsupported_version"}` | `v != 1` |
| Internal error | `{"ok": false, "error": "internal"}` | Logged at ERROR on the server; details are NOT leaked to the client |

**All application errors are published at QoS 1 with PUBACK reason `0x00 Success`.** The MQTT reason code reflects *transport-level* success (the message was delivered), not *application-level* success.

### 7.2 Transport errors (MQTT reason codes)

| Situation | MQTT reason | Notes |
|---|---|---|
| Authentication failure | CONNACK `0x87 (Not authorized)` | Session rejected at CONNECT; agent MUST NOT retry |
| Client ID taken | CONNACK `0x8E (Session taken over)` | Agent MUST regenerate `instance_uuid` and retry with a fresh ID |
| Server unreachable | TCP-level `ECONNREFUSED` / timeout | Agent retries with exponential backoff |
| Quota exceeded | PUBACK `0x97 (Quota exceeded)` | Reserved for a future per-tenant rate limit |
| Message too large | PUBACK `0x95 (Packet too large)` | 128 MB hard limit; requests above this are rejected |

### 7.3 Agent-side error handling map

The agent's `MqttTransportModule` classifies transport errors and calls the corresponding `ShutdownController` method:

| Condition | Agent action |
|---|---|
| CONNACK `0x87` | `ShutdownController::request(AuthRevoked)` — stop, do not retry |
| Repeated network failures (`N` consecutive, configurable, default 10) | `ShutdownController::request(FatalError)` — the broker is unreachable |
| `ServerRevoke` with `reason: "token_revoked"` | `ShutdownController::request(AuthRevoked)` |
| `ServerRevoke` with `reason: "agent_revoked"` | `ShutdownController::request(AgentRevoked)` |
| Response payload `{"error":"agent_revoked"}` | `ShutdownController::request(AgentRevoked)` |
| Response payload `{"error":"not_found"}` on heartbeat | Clear local `agent_id`, re-register |

---

## 8. Last Will & Testament

| Field | Value |
|---|---|
| Topic | `axiomdlp/sessions/{instance_uuid}/status` |
| Payload | `{"status":"offline"}` |
| QoS | 1 |
| Retain | **false** |
| Will Delay Interval | **5 s** |

### 8.1 Why 5 seconds

The 5-second Will Delay Interval tolerates brief network blips. If the agent reconnects within 5 seconds, the broker suppresses the LWT — no false "offline" signal. A real disconnect (crash, kill -9, network loss) triggers the LWT after 5 seconds.

### 8.2 Interaction with `SessionHooks.OnDisconnect`

The broker's `SessionHooks.OnDisconnect` fires **immediately** on session end, regardless of LWT. The LWT is therefore a **secondary** signal:

- **`SessionHooks.OnDisconnect`** — authoritative; drives `AgentHub.Unregister` and the SSE `EventAgent` `{status: "disconnected"}` event.
- **LWT** — informational; logs `{"msg":"mqtt lwt","instance_uuid":"...","status":"offline"}` and is used only to distinguish "network lost" from "graceful shutdown" in the SSE event's `reason` field.

A graceful `shutdown` request sets the SSE event's `reason` to `"user_requested"` (etc.); an LWT sets it to `"connection_lost"`.

### 8.3 Graceful disconnect

When the agent disconnects gracefully (shutdown flow), it:

1. Sends the `axiomdlp/agents/{agent_id}/shutdown` message with its reason.
2. Calls `mosquitto_disconnect_v5` with `Reason Code = 0x00 (Normal disconnection)`.
3. The broker does NOT fire the LWT.

The agent does **not** need to explicitly clear the LWT before disconnecting — the clean disconnect suppresses it automatically.

---

## 9. Message schemas

All messages carry `"v": 1`. Request messages also carry `"correlation_id": "<ulid>"`. Response messages echo the same `correlation_id`.

### 9.1 Register

**Request** → `axiomdlp/register`, QoS 1

```json
{
  "v": 1,
  "correlation_id": "01HZ9K8P2QX7M3N4V6W8Y0Z1B2",
  "hostname": "desktop-01",
  "os": "linux",
  "architecture": "x86_64",
  "agent_version": "0.1.0"
}
```

**Field rules:**

| Field | Type | Constraint |
|---|---|---|
| `v` | int | Must be `1` |
| `correlation_id` | string | ULID, 26 chars |
| `hostname` | string | 1–255 chars |
| `os` | string | 1–50 chars; `linux`, `darwin`, `windows` are recognized |
| `architecture` | string | 0–32 chars; `x86_64`, `i686`, `aarch64` are recognized |
| `agent_version` | string | 1–50 chars |

**Response** → agent's reply topic, QoS 1

```json
{
  "v": 1,
  "correlation_id": "01HZ9K8P2QX7M3N4V6W8Y0Z1B2",
  "ok": true,
  "agent_id": 42,
  "user_id": 7,
  "organization_id": 2,
  "status": "active",
  "server_time": "2026-11-09T13:00:00Z"
}
```

**Failure examples:**

```json
{"v":1,"correlation_id":"...","ok":false,"error":"invalid_request","details":{"hostname":"must be 1..255 characters"}}
```

### 9.2 Heartbeat

**Request** → `axiomdlp/agents/{agent_id}/heartbeat`, QoS 1

```json
{
  "v": 1,
  "correlation_id": "...",
  "agent_id": 42,
  "agent_version": "0.1.0"
}
```

`agent_version` is optional; when present it refreshes the stored value.

**Response**

```json
{"v":1,"correlation_id":"...","ok":true,"server_time":"2026-11-09T13:01:00Z"}
```

**Failure examples:**

```json
{"v":1,"correlation_id":"...","ok":false,"error":"not_found"}
{"v":1,"correlation_id":"...","ok":false,"error":"agent_revoked"}
```

### 9.3 ScanBatch

**Request** → `axiomdlp/agents/{agent_id}/scan/batch`, QoS 1

```json
{
  "v": 1,
  "correlation_id": "...",
  "agent_id": 42,
  "chunks": [
    {
      "file_hash": "b5bb9d8014a0f9b1d61e21e796d78dccdf1352f23cd32812f4850b878ae4944c",
      "path": "/home/user/doc.txt",
      "chunk_index": 0,
      "chunk_count": 3,
      "content_b64": "aGVsbG8gd29ybGQ="
    }
  ]
}
```

**Field rules:**

| Field | Type | Constraint |
|---|---|---|
| `chunks` | array | 1–500 items |
| `chunks[].file_hash` | string | 64 lowercase hex chars (SHA-256) |
| `chunks[].path` | string | 1–4096 chars |
| `chunks[].chunk_index` | int | `0 ≤ chunk_index < chunk_count` |
| `chunks[].chunk_count` | int | `≥ 1` |
| `chunks[].content_b64` | string | Base64 (RFC 4648, no URL-safe variant) |

**Response**

```json
{
  "v": 1,
  "correlation_id": "...",
  "ok": true,
  "results": [
    {
      "file_hash": "b5bb9d8014a0f9b1d61e21e796d78dccdf1352f23cd32812f4850b878ae4944c",
      "chunk_index": 0,
      "sensitive": true,
      "entity_type": "CREDIT_CARD",
      "risk_score": 8,
      "action": "block",
      "error": ""
    }
  ]
}
```

**Per-chunk errors** are reported in `error` on the corresponding `results[]` entry — the whole batch is not failed. The `sensitive`, `entity_type`, `risk_score`, and `action` fields are unset when `error` is non-empty.

**Result actions** (mirrors the server's policy engine verdicts):

| Action | Meaning |
|---|---|
| `"block"` | Policy verdict: block |
| `"alert"` | Policy verdict: alert |
| `"allow"` | No policy blocked; entity was sensitive but not denied |

### 9.4 Shutdown (no response)

**Request** → `axiomdlp/agents/{agent_id}/shutdown`, QoS 1

```json
{
  "v": 1,
  "agent_id": 42,
  "reason": "user_requested"
}
```

**Reason values:**

| Value | Meaning |
|---|---|
| `user_requested` | Ctrl+C, SIGTERM, service stop |
| `auth_revoked` | Token invalid or revoked server-side |
| `agent_revoked` | Agent itself revoked server-side |
| `config_error` | Bad config at startup |
| `fatal_error` | Unrecoverable runtime error |

**No response.** The server logs the shutdown, cleans up state, and does not reply. The agent's session ends when its TCP connection closes.

### 9.5 ServerCommand

**Topic** → `axiomdlp/agents/{agent_id}/command`, QoS 2, not retained

```json
{
  "v": 1,
  "command_type": "rescan",
  "payload_b64": "eyJzY29wZSI6Ii9ob21lIn0=",
  "issued_at": "2026-11-09T13:02:00Z",
  "trace_id": "01HZ..."
}
```

**Command types** (mirrors the `next.md` Phase 2 milestone 22):

| `command_type` | `payload_b64` decoded |
|---|---|
| `rescan` | `{"scope":"/path"}` — trigger a full or scoped inventory scan |
| `pause` | `{"duration_sec": 900}` — pause enforcement |
| `resume` | `{}` — resume enforcement |
| `quarantine_file` | `{"path": "/..."}` — move to vault |
| `dump_logs` | `{"lines": 1000}` — upload recent logs |
| `run_diagnostic` | `{}` — run self-test, upload results |
| `restart` | `{}` — restart the service |

`payload_b64` may be empty for commands with no payload. `trace_id` is optional; when present it propagates to the server's audit log.

### 9.6 ServerPolicyUpdate

**Topic** → `axiomdlp/agents/{agent_id}/policy`, QoS 2, **retained**

```json
{
  "v": 1,
  "version": 17,
  "bundle_b64": "...",
  "signature_b64": "...",
  "issued_at": "2026-11-09T13:03:00Z"
}
```

| Field | Type | Meaning |
|---|---|---|
| `version` | uint64 | Monotonic policy version counter. Higher wins |
| `bundle_b64` | string | Base64 of the signed policy bundle (JSON or CBOR — chosen by the Core) |
| `signature_b64` | string | Base64 of the Ed25519 signature over `bundle` |
| `issued_at` | RFC3339 string | Server-side issue time |

**Retain semantics:** the broker holds the latest policy bundle. An agent that reconnects receives it on subscribe. The agent compares `version` against its local cache and, if newer, verifies the signature against the embedded public key before applying.

**Clearing retained policy:** not supported in Phase 1 — the policy is always retained with the latest version. A future "policy revoked" milestone will publish an empty retained message to clear it.

### 9.7 ServerRevoke

**Topic** → `axiomdlp/agents/{agent_id}/revoke`, QoS 2, **retained**

```json
{"v":1,"reason":"token_revoked","message":"token revoked server-side"}
```

| `reason` | Meaning |
|---|---|
| `token_revoked` | The agent's API token was revoked server-side |
| `agent_revoked` | The agent itself was revoked server-side |

**Agent action on receipt:**

| `reason` | `ShutdownController::request(...)` |
|---|---|
| `token_revoked` | `AuthRevoked` |
| `agent_revoked` | `AgentRevoked` |

**Retain semantics:** the broker holds the latest revoke frame. An agent that reconnects while revoked receives it on subscribe and immediately shuts down. When the agent is un-revoked (administrative action), the server publishes an empty retained message to clear it.

### 9.8 Session status (LWT payload)

**Topic** → `axiomdlp/sessions/{instance_uuid}/status`, QoS 1, not retained

```json
{"status":"offline"}
```

The `reason` field is reserved for a future milestone that will distinguish `"network_lost"` from `"crashed"` from `"killed"`.

### 9.9 Common envelopes

Every request carries `"v": 1` and `"correlation_id": "<ulid>"`.

Every response carries `"v": 1`, `"correlation_id": "<ulid>"` (echoed), and `"ok": bool`. When `ok = false`, `"error"` is present.

Both sides MUST tolerate unknown fields when deserializing — this is the forward-compatibility mechanism that lets us add optional fields without a version bump. See §13.

---

## 10. End-to-end sequences

### 10.1 First-run registration

```
Agent                                            Server
 |                                                |
 |--- CONNECT (username=token) ------------------>|
 |                                                | AuthHook: hash → user_api_tokens lookup
 |<-- CONNACK 0x00 (Success) ---------------------|
 |                                                | SessionHooks: OnSessionEstablished (log only in Phase 1)
 |--- SUBSCRIBE axiomdlp/replies/{instance} ----->|
 |<-- SUBACK -------------------------------------|
 |                                                |
 |--- PUBLISH axiomdlp/register (QoS 1) --------->|
 |      properties:                               |
 |        Response-Topic = axiomdlp/replies/{inst}|
 |        Correlation-Data = <ulid>               |
 |                                                | handler: Register
 |                                                |   validate, agentRepo.Create
 |<-- PUBLISH axiomdlp/replies/{inst} (QoS 1) ----|
 |      properties:                               |
 |        Correlation-Data = <ulid>               |
 |      payload: {"ok":true,"agent_id":42,...}    |
 |                                                |
 | persist agent_id=42 to local state table       |
 |                                                |
 |--- SUBSCRIBE axiomdlp/agents/42/command ------>|
 |<-- SUBACK -------------------------------------|
 |--- SUBSCRIBE axiomdlp/agents/42/policy ------->|
 |<-- SUBACK (retained policy delivered now) -----|
 |--- SUBSCRIBE axiomdlp/agents/42/revoke ------->|
 |<-- SUBACK (retained revoke delivered if any) --|
 |                                                |
```

### 10.2 Steady-state heartbeat

```
Agent                                            Server
 |                                                |
 |--- PUBLISH axiomdlp/agents/42/heartbeat ------>|
 |      (QoS 1)                                   | handler: Heartbeat
 |      Response-Topic, Correlation-Data          |   agentRepo.UpdateHeartbeat
 |<-- PUBLISH axiomdlp/replies/{inst} (QoS 1) ----|
 |      payload: {"ok":true,"server_time":"..."}  |
 |                                                |
```

### 10.3 ScanBatch with policy verdict

```
Agent                                            Server
 |                                                |
 |--- PUBLISH axiomdlp/agents/42/scan/batch ----->|
 |      (QoS 1, up to 500 chunks)                 | handler: ScanBatch
 |                                                |   for each chunk:
 |                                                |     piiClient.DetectLong(content)
 |                                                |     policy.EvaluateEntities(...)
 |                                                |     eventRecorder.RecordResult(...)
 |<-- PUBLISH axiomdlp/replies/{inst} (QoS 1) ----|
 |      payload:                                   |
 |      {"ok":true,"results":[                     |
 |        {"file_hash":"...","chunk_index":0,      |
 |         "sensitive":true,"action":"block",...}  |
 |      ]}                                         |
 |                                                |
 | persist verdicts to local files table          |
 |                                                |
```

### 10.4 Server-initiated revoke (agent connected)

```
Admin (HTTP)                    Server                            Agent
 |                                |                                |
 |-- POST /agents/42/revoke ----->|                                |
 |                                | agentRepo.Revoke(id=42)        |
 |                                | AgentHub.RevokeByAgent:        |
 |                                |   publish axiomdlp/agents/42/revoke (QoS 2, retained)
 |                                |                                |<-- PUBLISH
 |                                |                                | ShutdownController(AuthRevoked)
 |                                |                                | send shutdown (QoS 1)
 |                                |                                | mosquitto_disconnect_v5 (clean)
 |                                |<-- PUBLISH axiomdlp/agents/42/shutdown
 |                                | OnSessionTerminated            |
 |                                | AgentHub.Unregister            |
 |                                | SSE EventAgent disconnected    |
```

### 10.5 Server-initiated policy update

```
Admin (HTTP)                    Server                            Agent
 |                                |                                |
 |-- POST /policies/... --------->|                                |
 |                                | policy bundle signed           |
 |                                | publish axiomdlp/agents/42/policy (QoS 2, retained)
 |                                |                                |<-- PUBLISH (retained)
 |                                |                                | verify signature (Ed25519)
 |                                |                                | version > local?
 |                                |                                | apply + hot-swap PolicyEngine
```

### 10.6 Network loss and reconnect

```
Agent                                            Server
 |                                                |
 |   (network drops)                              | SessionHooks: OnDisconnect (fires immediately)
 |                                                | AgentHub.Unregister
 |                                                | SSE EventAgent disconnected
 |                                                |
 |   (5 seconds pass, Will Delay Interval)        |
 |                                                | LWT fires: {"status":"offline"} on axiomdlp/sessions/{inst}/status
 |                                                |
 |   (agent reconnects with exponential backoff)  |
 |--- CONNECT (same Client ID, username=token) -->|
 |<-- CONNACK 0x00 -------------------------------|
 |--- SUBSCRIBE axiomdlp/replies/{inst} --------->|
 |<-- SUBACK -------------------------------------|
 |--- SUBSCRIBE axiomdlp/agents/42/command ------>|
 |<-- SUBACK -------------------------------------|
 |--- SUBSCRIBE axiomdlp/agents/42/policy ------->|
 |<-- SUBACK (retained policy re-delivered) ------|
 |--- SUBSCRIBE axiomdlp/agents/42/revoke ------->|
 |<-- SUBACK (retained revoke re-delivered if any)|
 |                                                |
 |--- PUBLISH heartbeat (QoS 1) ----------------->|
 |<-- response -----------------------------------|
```

If the reconnection happens within 5 seconds, the LWT is suppressed and no "offline" is ever published — the SSE stream simply does not emit a `disconnected` event because `OnDisconnect` did not fire either (session was Clean Start=false and reconnected within the same TCP session — MQTT 5 persistent sessions keep the subscription, so `OnSessionTerminated` is not triggered by transient TCP loss if the broker retains the session).

---

## 11. Migration mapping (gRPC → MQTT)

| Old gRPC | New MQTT |
|---|---|
| `AgentService.Register` (unary) | Publish `axiomdlp/register` + `Response-Topic` |
| `AgentService.Heartbeat` (unary) | Publish `axiomdlp/agents/{id}/heartbeat` + `Response-Topic` |
| `AgentService.ScanBatch` (unary) | Publish `axiomdlp/agents/{id}/scan/batch` + `Response-Topic` |
| `AgentService.Channel` (bidi) | Server → `command` / `policy` / `revoke`; Agent → `shutdown` |
| `ClientShutdown` (bidi frame) | `axiomdlp/agents/{id}/shutdown` (QoS 1) |
| `ServerCommand` (bidi frame) | `axiomdlp/agents/{id}/command` (QoS 2) |
| `ServerPolicyUpdate` (bidi frame) | `axiomdlp/agents/{id}/policy` (QoS 2, **retained**) |
| `ServerRevoke` (bidi frame) | `axiomdlp/agents/{id}/revoke` (QoS 2, **retained**) |
| Metadata `x-agent-id` | `{agent_id}` topic segment (verified per-message) |
| gRPC `UNAUTHENTICATED` | CONNACK `0x87` (session-level, no retry) |
| gRPC `PERMISSION_DENIED` | JSON `{"error":"agent_revoked"}` |
| gRPC `NOT_FOUND` | JSON `{"error":"not_found"}` |
| gRPC `INTERNAL` | JSON `{"error":"internal"}` |
| gRPC `UNAVAILABLE` / `DEADLINE_EXCEEDED` | TCP reconnect with backoff |
| `ClientShutdown.reason` enum | `shutdown.reason` string (same values) |
| `RevokeReason` enum | `ServerRevoke.reason` string |
| `AgentStatus` enum | `RegisterResponse.status` string |
| `protoc` + gRPC C++ | libmosquitto (C, portable) |
| Go `google.golang.org/grpc` | Mochi-MQTT v2 |

---

## 12. Deferred items (deliberate non-goals)

These are intentionally not solved in v1. Each has a natural insertion point later without a wire-version bump (they add optional fields or new topics).

| Item | Deferred to | Insertion point |
|---|---|---|
| Shared subscriptions (`$share/...`) for Core HA | Multi-instance milestone | Server-side subscription pattern only; no agent change |
| Per-token rate limiting on MQTT paths | When load tests demand it | Mochi `OnMessage` hook |
| CBOR / MsgPack serialization | If base64 overhead matters | An optional `"ct": "json\|cbor"` field on request envelope |
| Mutual TLS (agent certs) | If token-only auth proves insufficient | Client cert on TLS layer; no topic change |
| Dead-letter topic | If handler errors pile up | Server-side only; agents unaffected |
| Metrics via `$SYS` | Observability milestone | No agent change |
| Live policy clear | When un-publish is needed | Empty retained message on `policy` topic |
| Live token rotation | Post-Phase 2 | Server-published new token on `axiomdlp/agents/{id}/token` (new topic) |
| Topic aliases | If wire size matters | Client-side only; server accepts both forms |
| User properties (`User-Property` MQTT 5) | If tracing context needs richer propagation | Optional field on any message |

---

## 13. Versioning

### 13.1 Wire version

- The wire version is `1` and is carried in `"v"` on every message.
- **Breaking changes** bump `v` to `2`. Both sides must be updated in lockstep; a version-1 agent cannot talk to a version-2 server (and vice versa).
- **Non-breaking additions** keep `v = 1`. This includes:
  - New optional fields in request or response schemas.
  - New `command_type` values in `ServerCommand`.
  - New `error` values in responses.
  - New topics that agents MAY subscribe to but are not required to.
- Both sides **MUST tolerate unknown fields** when deserializing. JSON decoders that error on unknown fields are forbidden.

### 13.2 Document version

This document is versioned alongside the wire version. Section changes that alter wire semantics bump `v`. Section changes that only clarify existing behavior do not.

### 13.3 Two-repo contract

This document is authoritative in **both** repositories:

- `axiomdlp-core/docs/mqtt_protocol.md`
- `axiomdlp-agent/docs/mqtt_protocol.md`

The two files MUST be byte-identical. A change here is a change in both. CI enforcement:

```bash
diff -q axiomdlp-core/docs/mqtt_protocol.md axiomdlp-agent/docs/mqtt_protocol.md
```

This check runs in Phase 6's CI setup and MUST fail the build on drift.

---

## 14. Reference implementation

| Concern | Server (Go) | Agent (C++) |
|---|---|---|
| Broker / client | `internal/mqtt/broker.go` (Mochi-MQTT v2) | `src/features/mqtt_transport/mqtt_transport_module.{h,cpp}` (libmosquitto) |
| Auth | `internal/mqtt/auth.go` (AuthHook) | N/A — client sends username |
| Session lifecycle | `internal/mqtt/hooks.go` (SessionHooks) | N/A — client observes connect/disconnect |
| Wire structs | `internal/mqtt/messages.go` | nlohmann/json inline structs in the transport module |
| Topics + QoS | `internal/mqtt/topics.go` | Constants in `mqtt_transport_module.cpp` |
| Message handlers | `internal/mqtt/handlers_*.go` (Phase 2) | The callers (`agent_sync`, `content_scanner`) call `MqttTransportModule::request()` |

### 14.1 Server-side message routing

The server subscribes to all patterns in §4.1 at broker start (see `internal/mqtt/topics.go` → `AllServerSubscriptions`). Each incoming PUBLISH is dispatched to a per-family handler in `internal/mqtt/handlers_*.go`. The handler:

1. Reads `Correlation-Data` and `Response-Topic` from the PUBLISH properties.
2. Parses the JSON payload into the typed struct from `messages.go`.
3. Validates the payload with `Validate()`.
4. Runs the business logic (agentRepo, piiClient, policy enforcer, event recorder).
5. Publishes the response on `Response-Topic` with the same `Correlation-Data`.

### 14.2 Agent-side request flow

```
agent_sync                content_scanner
    |                          |
    |                          |
    +------- request() --------+-----> MqttTransportModule
                                          |
                                          | generate ULID
                                          | publish + wait
                                          |
                                          v
                                     libmosquitto → broker → handler
                                          ^
                                          |
                                          | reply
                                          |
                                     on_message → route by correlation_id
```

Both callers use the same synchronous `request()` API. This is deliberately identical to the old gRPC unary call shape, so `agent_sync_module.cpp` and `content_scanner_module.cpp` migrate with mechanical changes only.

---

## 15. Verification checklist

A Phase-1 broker passes all of these. Any failure is a bug in the protocol implementation, not the protocol itself.

### 15.1 Broker reachable

```bash
ss -tlnp | grep 1883
```

Expected: a LISTEN line on `:1883`.

### 15.2 Invalid token rejected

```bash
mosquitto_pub -h localhost -p 1883 -u "axdlp_user_bogus" -P "" \
  -t 'axiomdlp/register' -m '{}'
```

Expected: `Connection error: Connection Refused: not authorised.`

Server log: `{"msg":"mqtt auth: unknown or revoked token",...}`

### 15.3 Valid token accepted

```bash
mosquitto_sub -h localhost -p 1883 -u "$TOKEN" -P "" \
  -t 'axiomdlp/replies/test' -v
```

Expected server logs:

```
{"msg":"mqtt auth: authenticated","client_id":"...","user_id":N,"org_id":M,"token_id":K,...}
{"msg":"mqtt session: established","client_id":"...","user_id":N,"org_id":M,"token_id":K,"agent_id":0,...}
```

Ctrl+C the subscriber → server log:

```
{"msg":"mqtt session: terminated","client_id":"...","user_id":N,"org_id":M,...}
```

### 15.4 Wildcard subscribe rejected

```bash
mosquitto_sub -h localhost -p 1883 -u "$TOKEN" -P "" \
  -t 'axiomdlp/agents/+/heartbeat' -v
```

Expected server log: `{"msg":"mqtt acl: wildcard subscription rejected",...}`

### 15.5 Health endpoint

```bash
curl -s http://localhost:3000/health | jq .mqtt
```

Expected:

```json
{"enabled": true, "port": 1883, "tls": false, "broker_running": true}
```

### 15.6 Graceful shutdown

SIGINT the server → expected logs:

```
{"msg":"Termination sequence received. Flushing buffer connections..."}
{"msg":"mqtt: broker stopped"}
{"msg":"Process context closed cleanly. System offline."}
```

---

## 16. Security considerations

### 16.1 Threat model

| Threat | Mitigation |
|---|---|
| Attacker steals a `axdlp_user_*` token | Token is scoped to one user's agent(s). Broker ACL prevents cross-agent topic access. Rate limiting (deferred, §12) will further constrain abuse. |
| Attacker replays an old token | Tokens have no expiry in Phase 1; revocation is via `POST /me/tokens/:id` (see core API). Phase 2 will add periodic token checks. |
| Attacker subscribes to `axiomdlp/agents/+/...` | Rejected by ACL (§4.5). |
| Attacker publishes to another agent's `shutdown` topic | Rejected by a Phase 3 ACL rule (pending). Phase 1 does not enforce this because no agent is connected yet. |
| MITM on the network | TLS 1.2+ required in production (§2.1). |
| Compromised broker process | Broker runs in the Core process; the Core's existing security posture applies (RLS, encrypted secrets at rest, audit log). |
| Replayed `ServerPolicyUpdate` | Signature verification with the embedded Ed25519 public key rejects any bundle not signed by the Core's release key. Version counter prevents downgrade. |

### 16.2 What is NOT protected in Phase 1

- **Cross-agent write isolation** is deferred to Phase 3's per-agent ACL rule. Phase 1 allows any authenticated client to publish to any topic. Agents only start connecting in Phase 7+; the window is deliberate.
- **Token rotation without restart** is deferred. Revoking a token disconnects the corresponding agent at the next `SessionHooks` event but does not proactively close active sessions (that is Phase 4's job).
- **Message signing beyond policy** is deferred. Heartbeats, scan batches, and shutdown messages are not signed. Integrity is provided by TLS.

### 16.3 What the agent MUST NOT do

- MUST NOT log the plaintext token. Only the prefix (first 20 chars) is safe to log.
- MUST NOT write the token to disk in plaintext outside its config file. The config file is the operator's responsibility to protect.
- MUST NOT subscribe with `+` or `#` under `axiomdlp/agents/` or `axiomdlp/replies/`.
- MUST NOT publish to another agent's topics (Phase 3 will enforce this server-side; Phase 1 relies on the agent being well-behaved).
- MUST NOT ignore signature verification for `ServerPolicyUpdate`. Every bundle must be verified against the embedded public key before it affects enforcement.
```

---

## Placement

```bash
# Server repo
cp mqtt_protocol.md /home/kali/axiomdlp-core/docs/mqtt_protocol.md

# Agent repo (byte-identical)
cp mqtt_protocol.md /home/kali/axiomdlp-agent/docs/mqtt_protocol.md

# Verify
diff -q /home/kali/axiomdlp-core/docs/mqtt_protocol.md \
        /home/kali/axiomdlp-agent/docs/mqtt_protocol.md
# Expected: no output
```

The document is now the authoritative contract. Every future server and agent change is checked against it. Sections 4 (topics), 5 (QoS), 6 (correlation), 9 (schemas), and 11 (migration) are the ones implementation code must match exactly; the rest is context and rationale.