#!/usr/bin/env bash
#
# scripts/build-all.sh — Linux-only build entrypoint.
#
# Windows builds are done natively on Windows via
#   scripts/build-windows-msvc.ps1   (MSVC + vcpkg)
# See docs/windows-msvc-build.md.
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS_DIR="${ROOT}/.deps"

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

openssl_target_ready() {
    local dir="$1"
    { [ -f "${dir}/lib/libssl.a"   ] && [ -f "${dir}/lib/libcrypto.a"   ]; } || \
    { [ -f "${dir}/lib64/libssl.a" ] && [ -f "${dir}/lib64/libcrypto.a" ]; }
}

if ! openssl_target_ready "${DEPS_DIR}/openssl-linux-x86_64" || \
   ! openssl_target_ready "${DEPS_DIR}/openssl-linux-i686"; then
    echo "==============================================="
    echo "OpenSSL not fully built; building (one-time)..."
    echo "==============================================="
    "${ROOT}/scripts/build-openssl.sh"
    echo
else
    echo "OpenSSL: already built for all Linux targets, skipping."
    echo
fi

BUILD_TYPE="${1:-release}"

TARGETS=(
    linux-x86_64
    linux-i686
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
echo "All Linux builds complete. Artifacts:"
echo "==============================================="

for target in "${TARGETS[@]}"; do
    file "build/${target}-${BUILD_TYPE}/axiomdlp-agent"
done

echo
echo "For Windows: clone on a Windows box and run scripts\\build-windows-msvc.ps1"
