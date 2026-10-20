#!/usr/bin/env python3
import subprocess
import time
import json
import os
import signal
import sys
import threading

# Paths
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
AGENT_BIN = os.path.join(ROOT_DIR, "build/linux-x86_64-release/axiomdlp-agent")
DEFAULT_CONFIG = os.path.join(ROOT_DIR, "config/agent.default.json")
TEST_CONFIG = os.path.join(ROOT_DIR, "config/agent.test.json")

def write_config(transport, base_url, tls=False, ca_path=""):
    if not os.path.exists(DEFAULT_CONFIG):
        print(f"❌ Error: {DEFAULT_CONFIG} not found.")
        sys.exit(1)
        
    with open(DEFAULT_CONFIG, "r") as f:
        cfg = json.load(f)
    
    cfg["server"]["transport"] = transport
    cfg["server"]["base_url"] = base_url
    cfg["server"]["grpc_tls"] = tls
    cfg["server"]["grpc_ca_path"] = ca_path
    
    # Toggle modules based on transport
    cfg["modules"]["grpc_transport"]["enabled"] = (transport == "grpc")
    cfg["modules"]["transport"]["enabled"] = (transport == "websocket")
    
    if not cfg["server"]["api_token"]:
        cfg["server"]["api_token"] = "axdlp_user_dummy_token_for_testing"

    with open(TEST_CONFIG, "w") as f:
        json.dump(cfg, f, indent=2)

def run_agent_and_capture_logs(timeout=4):
    if not os.path.exists(AGENT_BIN):
        print(f"❌ Error: {AGENT_BIN} not found. Build the project first.")
        sys.exit(1)

    process = subprocess.Popen(
        [AGENT_BIN, "--config", TEST_CONFIG, "--foreground"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        cwd=ROOT_DIR
    )
    
    logs = []
    
    # Start a background thread to read stdout without blocking the main thread
    def reader():
        for line in process.stdout:
            logs.append(line.strip())
            
    t = threading.Thread(target=reader, daemon=True)
    t.start()
    
    # Sleep for the timeout duration
    time.sleep(timeout)
    
    # Terminate the process gracefully
    if process.poll() is None:
        process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            
    t.join(timeout=1)
    return "\n".join(logs)

def test_grpc_smoke():
    print("\n=== Test 1: gRPC Initialization ===")
    write_config("grpc", "http://127.0.0.1:50051", tls=False)
    logs = run_agent_and_capture_logs()
    
    assert "grpc_transport module: initialized" in logs, "gRPC module did not initialize"
    assert "agent_sync module: initialized (transport=grpc" in logs, "agent_sync did not use gRPC"
    print("✅ gRPC modules initialized correctly")

def test_websocket_smoke():
    print("\n=== Test 2: WebSocket Initialization (Dual-Run) ===")
    write_config("websocket", "http://127.0.0.1:3000")
    logs = run_agent_and_capture_logs()
    
    assert "transport module: initialized" in logs, "WS module did not initialize"
    assert "agent_sync module: initialized (transport=websocket" in logs, "agent_sync did not use WS"
    print("✅ WebSocket modules initialized correctly")

def test_tls_failure():
    print("\n=== Test 3: TLS Misconfiguration Rejection ===")
    write_config("grpc", "https://127.0.0.1:50051", tls=True, ca_path="/non/existent/path/ca.crt")
    logs = run_agent_and_capture_logs()
    
    # The agent should try to read the CA and fail
    assert "failed to load CA" in logs or "cannot open" in logs or "Connection refused" in logs, "Agent did not reject bad TLS config"
    print("✅ TLS misconfiguration rejected correctly")

if __name__ == "__main__":
    print("Starting Phase 6 Python Test Suite...\n")
    try:
        test_grpc_smoke()
        test_websocket_smoke()
        test_tls_failure()
        print("\n✅ All Python smoke tests passed.")
    except AssertionError as e:
        print(f"\n❌ Test Failed: {e}")
        sys.exit(1)
    finally:
        if os.path.exists(TEST_CONFIG):
            os.remove(TEST_CONFIG)