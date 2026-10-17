#!/usr/bin/env bash
# scripts/build-grpc.sh
#
# Builds gRPC statically for Linux targets only.
#
# Windows (MinGW) is intentionally omitted:
#   gRPC has no official MinGW support. The MSYS2 patches exist but
#   are version-locked and break on newer releases. For Windows,
#   the agent uses its existing Boost.Beast WebSocket + HTTP
#   transport. Windows gRPC is deferred until we have a real
#   MSVC build machine.
#
# Outputs (reused if already present — no forced rebuild):
#   .deps/grpc-linux-x86_64/
#   .deps/grpc-linux-i686/
#
# Prerequisites:
#   - OpenSSL built for both Linux targets (run build-openssl.sh)
#   - For i686: libsystemd-dev:i386 (build machine only)
#
# First run takes 30-60 min (Linux x86_64 builds everything from
# scratch). Later runs are instant — target_ready() skips completed
# trees.

set -euo pipefail

GRPC_VERSION="v1.62.0"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS="$ROOT/.deps"
SRC="$DEPS/grpc-src"
BUILD_DIR="$DEPS/grpc-build"

export CMAKE_POLICY_VERSION_MINIMUM=3.5

mkdir -p "$DEPS"

# ----------------------------------------------------------------
# Download source + submodules (once)
# ----------------------------------------------------------------
if [ ! -d "$SRC" ]; then
    echo "Downloading gRPC $GRPC_VERSION with submodules..."
    git clone --depth 1 --branch "$GRPC_VERSION" \
        --recurse-submodules --shallow-submodules \
        https://github.com/grpc/grpc.git "$SRC"
else
    echo "gRPC source already cached at $SRC"
fi

# ----------------------------------------------------------------
# Readiness check — used to skip already-built trees
# ----------------------------------------------------------------
target_ready() {
    local dir="$1"
    [ -f "${dir}/lib/libgrpc++.a" ] && \
    [ -f "${dir}/lib/libgrpc.a"   ] && \
    [ -f "${dir}/lib/libprotobuf.a" ]
}

# ----------------------------------------------------------------
# Host tools (protoc, grpc_cpp_plugin)
#
# Needed for code generation on every target. The i686 build skips
# its own codegen (see below), so these host tools are the only
# source of protoc/plugin for 32-bit builds.
# ----------------------------------------------------------------
HOST_TOOLS="$DEPS/grpc-host-tools"

if [ ! -f "$HOST_TOOLS/bin/protoc" ]; then
    echo "Building host tools (protoc, grpc_cpp_plugin)..."
    mkdir -p "$HOST_TOOLS" "$BUILD_DIR/host"
    pushd "$BUILD_DIR/host" >/dev/null
    cmake "$SRC" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$HOST_TOOLS" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON \
        -DgRPC_BUILD_CSHARP_EXT=OFF \
        -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
        -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF \
        -DgRPC_INSTALL=ON \
        -DgRPC_SSL_PROVIDER=package \
        -DOPENSSL_ROOT_DIR="$DEPS/openssl-linux-x86_64"
    make -j"$(nproc)" protoc grpc_cpp_plugin
    make install
    popd >/dev/null
else
    echo "Host tools already built at $HOST_TOOLS"
fi

# ----------------------------------------------------------------
# Build one target.
# Args: <target-name> <cmake-toolchain-or-""> <openssl-dir> [extra flags...]
# ----------------------------------------------------------------
build_one() {
    local name="$1"
    local toolchain="$2"
    local openssl_dir="$3"
    shift 3
    local extra_args=("$@")

    local out="$DEPS/grpc-$name"

    if target_ready "$out"; then
        echo "gRPC for $name already built at $out; skipping."
        return
    fi

    echo "Building gRPC for $name"

    local build_subdir="$BUILD_DIR/$name"
    mkdir -p "$build_subdir"
    pushd "$build_subdir" >/dev/null

    local cmake_args=(
        "$SRC"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX="$out"
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        -DgRPC_BUILD_TESTS=OFF
        -DgRPC_INSTALL=ON
        -DBUILD_SHARED_LIBS=OFF
        -DgRPC_SSL_PROVIDER=package
        -DOPENSSL_ROOT_DIR="$openssl_dir"
        -DgRPC_ZLIB_PROVIDER=module
        -DgRPC_CARES_PROVIDER=module
        -DgRPC_RE2_PROVIDER=module
        -DgRPC_ABSL_PROVIDER=module
        -DgRPC_PROTOBUF_PROVIDER=module
        -DgRPC_PROTOBUF_PACKAGE_TYPE=MODULE
        -DgRPC_BUILD_GRPC_CPP_PLUGIN=ON
        -DgRPC_BUILD_CSHARP_EXT=OFF
        -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
    )

    if [ -n "$toolchain" ]; then
        cmake_args+=( "-DCMAKE_TOOLCHAIN_FILE=$toolchain" )
    fi

    cmake_args+=( "${extra_args[@]}" )

    cmake "${cmake_args[@]}"
    make -j"$(nproc)"
    make install
    popd >/dev/null

    echo "gRPC for $name installed at $out"
}

# ----------------------------------------------------------------
# Linux x86_64 (native)
# ----------------------------------------------------------------
build_one "linux-x86_64" \
    "" \
    "$DEPS/openssl-linux-x86_64"

# ----------------------------------------------------------------
# Linux i686 (32-bit)
#
# Notes:
#   - gRPC_BUILD_CODEGEN=OFF: the codegen plugin fails to compile
#     on 32-bit. Host tools (built above) handle codegen instead.
#   - gRPC_SYSTEMD_PROVIDER=none: without this, CMake bakes the
#     host's 64-bit /usr/lib/x86_64-linux-gnu/libsystemd.so into
#     gRPCTargets.cmake, and 32-bit consumers fail to link with
#     "file in wrong format". The agent's own systemd support
#     comes from CMakeLists.txt via pkg-config — gRPC does not
#     need its own systemd integration.
# ----------------------------------------------------------------
build_one "linux-i686" \
    "$ROOT/cmake/toolchains/linux-i686.cmake" \
    "$DEPS/openssl-linux-i686" \
    -DgRPC_BUILD_CODEGEN=OFF \
    -DgRPC_SYSTEMD_PROVIDER=none

# ----------------------------------------------------------------
# Summary
# ----------------------------------------------------------------
echo
echo "All gRPC builds complete."
echo
echo "Status:"
for target in linux-x86_64 linux-i686; do
    d="$DEPS/grpc-$target"
    if target_ready "$d"; then
        echo "  ✓ $d"
    else
        echo "  ✗ $d (missing libraries)"
    fi
done

echo
echo "Windows gRPC is intentionally not built here."
echo "The agent uses Boost.Beast WebSocket + JSON on Windows."
