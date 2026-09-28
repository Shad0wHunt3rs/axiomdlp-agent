#!/usr/bin/env bash
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
    { [ -f "${dir}/lib/libssl.a"    ] && [ -f "${dir}/lib/libcrypto.a"    ]; } || \
    { [ -f "${dir}/lib64/libssl.a"  ] && [ -f "${dir}/lib64/libcrypto.a"  ]; }
}

if ! openssl_target_ready "${DEPS_DIR}/openssl-linux-x86_64" || \
   ! openssl_target_ready "${DEPS_DIR}/openssl-linux-i686"   || \
   ! openssl_target_ready "${DEPS_DIR}/openssl-mingw64"      || \
   ! openssl_target_ready "${DEPS_DIR}/openssl-mingw32"; then
    echo "==============================================="
    echo "OpenSSL not fully built; building (one-time)..."
    echo "==============================================="
    "${ROOT}/scripts/build-openssl.sh"
    echo
else
    echo "OpenSSL: already built for all targets, skipping."
    echo
fi

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
