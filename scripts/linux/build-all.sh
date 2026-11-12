#!/usr/bin/env bash
#
# scripts/linux/build-all.sh — Linux-only build entrypoint.
#
# Windows builds are done natively on Windows via
#   scripts/win/build-windows-msvc.ps1   (MSVC + vcpkg)
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS_DIR="${ROOT}/.deps"

# ----------------------------------------------------------------
# Toolchain / dependency pre-checks
# ----------------------------------------------------------------
echo "==============================================="
echo "Checking build prerequisites"
echo "==============================================="

for tool in git cmake ninja pkg-config make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: '$tool' is not on PATH." >&2
        echo "       Install it with:  sudo apt install $tool" >&2
        exit 1
    fi
done

# libmosquitto is the agent transport (Phase 7+). Required to build
# the MQTT transport module. Without it, the build fails and the
# agent has no live channel to the Core.
if ! pkg-config --exists libmosquitto; then
    cat >&2 <<'EOF'
ERROR: libmosquitto is not installed (pkg-config cannot find it).

Install it with:
    sudo apt install libmosquitto-dev

For the i686 target, also run:
    sudo dpkg --add-architecture i386
    sudo apt update
    sudo apt install libmosquitto-dev:i386

Verify:
    pkg-config --modversion libmosquitto

If you want to build WITHOUT MQTT (Phase 7 module disabled), pass:
    AXIOM_USE_MQTT_OFF=1 ./scripts/linux/build-all.sh
EOF
    if [ "${AXIOM_USE_MQTT_OFF:-0}" = "1" ]; then
        echo "Continuing with AXIOM_USE_MQTT=OFF (no MQTT transport)."
    else
        exit 1
    fi
fi

if pkg-config --exists libmosquitto; then
    echo "libmosquitto: $(pkg-config --modversion libmosquitto)"
fi
echo

# ----------------------------------------------------------------
# nlohmann/json source cache
# ----------------------------------------------------------------
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
# OpenSSL
# ----------------------------------------------------------------
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
    "${ROOT}/scripts/linux/build-openssl.sh"
    echo
else
    echo "OpenSSL: already built for all Linux targets, skipping."
    echo
fi

# ----------------------------------------------------------------
# Configure + build
# ----------------------------------------------------------------
BUILD_TYPE="${1:-release}"

TARGETS=(
    linux-x86_64
    linux-i686
)

# Forward AXIOM_USE_MQTT_OFF into the CMake configure step as
# -DAXIOM_USE_MQTT=OFF if the user requested it.
CMAKE_EXTRA_ARGS=()
if [ "${AXIOM_USE_MQTT_OFF:-0}" = "1" ]; then
    CMAKE_EXTRA_ARGS=(-DAXIOM_USE_MQTT=OFF)
fi

for target in "${TARGETS[@]}"; do
    preset="${target}-${BUILD_TYPE}"
    echo "==============================================="
    echo "Building: ${preset}"
    echo "==============================================="
    cmake --preset "${preset}" "${CMAKE_EXTRA_ARGS[@]}"
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
echo "For Windows: clone on a Windows box and run scripts\\win\\build-windows-msvc.ps1"