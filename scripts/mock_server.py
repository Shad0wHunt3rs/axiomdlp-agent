#!/usr/bin/env python3
"""
Mock WebSocket server for testing the TransportModule.

Listens on ws://127.0.0.1:8765, logs every message, and responds to
heartbeats with heartbeat_ack.

Requirements:
    pip install websockets
"""

import asyncio
import json
import signal
import sys

try:
    import websockets
except ImportError:
    print("ERROR: 'websockets' package not installed.", file=sys.stderr)
    print("Install:  pip install websockets", file=sys.stderr)
    sys.exit(1)

HOST = "127.0.0.1"
PORT = 8765

# Track stats for a periodic summary.
_stats = {"heartbeats": 0, "sensitive_files": 0}


async def handle_connection(ws):
    peer = ws.remote_address
    print(f"[+] client connected: {peer}")

    try:
        async for raw in ws:
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                print(f"[!] malformed JSON: {raw[:120]}")
                continue

            msg_type = msg.get("type", "?")
            payload = msg.get("payload", {})

            if msg_type == "heartbeat":
                _stats["heartbeats"] += 1
                await ws.send(json.dumps({
                    "v": 1,
                    "type": "heartbeat_ack",
                    "ts": msg.get("ts", 0),
                    "payload": {},
                }))
                print(f"[<] heartbeat #{_stats['heartbeats']}")

            elif msg_type == "sensitive_file":
                _stats["sensitive_files"] += 1
                path = payload.get("path", "?")
                cat = payload.get("category", "?")
                size = payload.get("size", 0)
                print(f"[<] sensitive #{_stats['sensitive_files']}: "
                      f"{path} ({cat}, {size} bytes)")

            else:
                print(f"[<] {msg_type}: {json.dumps(payload)[:200]}")

    except websockets.ConnectionClosed:
        pass
    finally:
        print(f"[-] client disconnected: {peer}")


async def main():
    stop_event = asyncio.Event()

    def on_signal(*_):
        print("\nshutting down...")
        stop_event.set()

    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, on_signal)
        except NotImplementedError:
            pass

    async with websockets.serve(handle_connection, HOST, PORT):
        print(f"mock server listening on ws://{HOST}:{PORT}")
        print("Press Ctrl+C to stop.\n")
        await stop_event.wait()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass