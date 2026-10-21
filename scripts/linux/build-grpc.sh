#!/usr/bin/env bash
# scripts/build-grpc.sh
#
# Builds gRPC statically for Linux targets only.
#
# Windows (MinGW) is intentionally omitted:
#   gRPC has no official MinGW support. The MSYS2 patches exist but
#   are version-locked and break on newer releases. Windows gRPC is
#   supplied by vcpkg on the MSVC build path.
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
#
# Cross-architecture note:
#   When building i686 on a 64-bit host, any dependency resolved via
#   find_package(OpenSSL) or find_package(Brotli) will silently pick
#   the host's 64-bit library unless the search is explicitly pinned.
#   The two settings that matter:
#
#     -DgRPC_BROTLI_PROVIDER=module   bundle brotli from third_party
#     -DOPENSSL_SSL_LIBRARY / -DOPENSSL_CRYPTO_LIBRARY
#                                     pin OpenSSL to .deps (correct arch)
#
#   Without these, gRPCTargets.cmake ends up containing absolute
#   /usr/lib/x86_64-linux-gnu/... paths, and the 32-bit link fails
#   with "error adding symbols: file in wrong format".

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

    # Determine where OpenSSL installed its static libs (lib/ or lib64/).
    # OpenSSL 3 puts libs in lib64/ on 64-bit and lib/ on 32-bit.
    local openssl_libdir=""
    if [ -f "$openssl_dir/lib/libssl.a" ]; then
        openssl_libdir="$openssl_dir/lib"
    elif [ -f "$openssl_dir/lib64/libssl.a" ]; then
        openssl_libdir="$openssl_dir/lib64"
    else
        echo "Error: OpenSSL static libs not found under $openssl_dir" >&2
        return 1
    fi

    echo "Building gRPC for $name"
    echo "  OpenSSL root:  $openssl_dir"
    echo "  OpenSSL libs:  $openssl_libdir"

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
        -DOPENSSL_INCLUDE_DIR="$openssl_dir/include"
        -DOPENSSL_SSL_LIBRARY="$openssl_libdir/libssl.a"
        -DOPENSSL_CRYPTO_LIBRARY="$openssl_libdir/libcrypto.a"
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
#
# System brotli and system OpenSSL are x86_64, matching the target,
# so no provider pinning is required beyond OPENSSL_ROOT_DIR.
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
#     "file in wrong format".
#   - gRPC_BROTLI_PROVIDER=module: gRPC's default brotli provider
#     is "package", which does find_package(Brotli). On a 64-bit
#     host without libbrotli-dev:i386 installed, that resolves to
#     /usr/lib/x86_64-linux-gnu/libbrotli*.so and links 64-bit
#     code into a 32-bit binary. "module" bundles brotli from
#     gRPC's own third_party/brotli submodule instead.
#   - OPENSSL_SSL_LIBRARY / OPENSSL_CRYPTO_LIBRARY: OPENSSL_ROOT_DIR
#     alone is not always enough on multiarch Debian hosts, where
#     FindOpenSSL can still prefer the host's 64-bit libssl.so.
#     Pinning the exact library paths removes the ambiguity.
# ----------------------------------------------------------------
build_one "linux-i686" \
    "$ROOT/cmake/toolchains/linux-i686.cmake" \
    "$DEPS/openssl-linux-i686" \
    -DgRPC_BUILD_CODEGEN=OFF \
    -DgRPC_SYSTEMD_PROVIDER=none \
    -DgRPC_BROTLI_PROVIDER=module \
    -DgRPC_ZSTD_PROVIDER=module

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
        echo "  [OK] $d"
    else
        echo "  [FAIL] $d (missing libraries)"
    fi
done

echo
echo "Windows gRPC is supplied by vcpkg on the MSVC build path."
echo "See scripts/win/build-windows-msvc.ps1."