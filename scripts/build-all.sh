#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-release}"

TARGETS=(
    linux-x86_64
    linux-i686
    windows-x86_64
    windows-i686
)

for target in "${TARGETS[@]}"; do
    preset="${target}-${BUILD_TYPE}"
    echo "==============================================="
    echo "Building: ${preset}"
    echo "==============================================="
    cmake --preset "${preset}"
    cmake --build --preset "${preset}"
done

echo
echo "==============================================="
echo "All builds complete. Artifacts:"
echo "==============================================="
for target in "${TARGETS[@]}"; do
    if [[ "$target" == windows-* ]]; then
        f="build/${target}-${BUILD_TYPE}/axiomdlp-agent.exe"
    else
        f="build/${target}-${BUILD_TYPE}/axiomdlp-agent"
    fi
    file "$f"
done