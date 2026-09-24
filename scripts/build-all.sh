#!/usr/bin/env bash
set -euo pipefail

# ----------------------------------------------------------------
# One-time: populate the nlohmann_json source cache.
#
# FetchContent needs the source available for cross-compiles (Windows
# builds), but cloning from GitHub takes ~13 minutes. We pre-populate
# a local cache so the first clone is reused forever.
# ----------------------------------------------------------------
DEPS_DIR="$(cd "$(dirname "$0")/.." && pwd)/.deps"

if [ ! -d "${DEPS_DIR}/nlohmann_json-src" ]; then
    echo "==============================================="
    echo "Populating nlohmann_json source cache (one-time)..."
    echo "==============================================="
    mkdir -p "${DEPS_DIR}"
    git clone --depth 1 --branch v3.11.3 \
        https://github.com/nlohmann/json.git \
        "${DEPS_DIR}/nlohmann_json-src"
    echo
fi

# ----------------------------------------------------------------
# Build every configured target.
# ----------------------------------------------------------------
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

# ----------------------------------------------------------------
# Report artifacts.
# ----------------------------------------------------------------
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