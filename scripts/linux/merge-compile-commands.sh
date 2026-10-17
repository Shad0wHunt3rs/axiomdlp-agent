#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/compile_commands.json"

FILES=(
    "$ROOT/build/linux-x86_64-debug/compile_commands.json"
    "$ROOT/build/linux-x86_64-release/compile_commands.json"
    "$ROOT/build/linux-i686-release/compile_commands.json"
    "$ROOT/build/windows-x86_64-debug/compile_commands.json"
    "$ROOT/build/windows-x86_64-release/compile_commands.json"
    "$ROOT/build/windows-i686-release/compile_commands.json"
)

python3 - "${FILES[@]}" "$OUT" << 'PYEOF'
import json, sys, os
out = sys.argv[-1]
inputs = sys.argv[1:-1]
merged = {}
for path in inputs:
    if not os.path.exists(path):
        continue
    with open(path) as f:
        for entry in json.load(f):
            merged[entry["file"]] = entry
result = sorted(merged.values(), key=lambda e: e["file"])
with open(out, "w") as f:
    json.dump(result, f, indent=2)
print(f"Merged {len(result)} entries into {out}")
PYEOF
