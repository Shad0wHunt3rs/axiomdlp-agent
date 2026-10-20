#!/usr/bin/env bash
# scripts/build-openssl.sh
#
# Builds OpenSSL statically for both Linux agent targets.
# Windows OpenSSL is supplied by vcpkg on the Windows side.
# Output goes to:
#   .deps/openssl-linux-x86_64/
#   .deps/openssl-linux-i686/
#
# Run once. Re-run only if you bump OPENSSL_VERSION.
# Takes 15-25 minutes total.
#
# Built with no-zlib so that FindOpenSSL.cmake does not try to add
# ZLIB::ZLIB to the link interface of OpenSSL::SSL / OpenSSL::Crypto.
# Without this, the i686 build fails at configure time with
# "The link interface of target OpenSSL::Crypto contains ZLIB::ZLIB
#  but the target was not found" unless zlib1g-dev:i386 is installed.
# TLS compression has been disabled by default since 2012 for
# security reasons (CRIME / BREACH), so nothing is lost.

set -euo pipefail

OPENSSL_VERSION="3.3.2"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS="$ROOT/.deps"
SRC="$DEPS/openssl-src"
TARBALL="$DEPS/openssl.tar.gz"

mkdir -p "$DEPS"

# ---------- Download source once ----------
if [ ! -d "$SRC" ]; then
    echo "Downloading OpenSSL $OPENSSL_VERSION..."
    curl -fL --retry 3 \
        -o "$TARBALL" \
        "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz"
    tar -xzf "$TARBALL" -C "$DEPS"
    mv "$DEPS/openssl-$OPENSSL_VERSION" "$SRC"
    rm -f "$TARBALL"
else
    echo "OpenSSL source already cached at $SRC"
fi

# Returns 0 if libssl.a and libcrypto.a exist in lib/ or lib64/.
target_ready() {
    local dir="$1"
    { [ -f "${dir}/lib/libssl.a"   ] && [ -f "${dir}/lib/libcrypto.a"   ]; } || \
    { [ -f "${dir}/lib64/libssl.a" ] && [ -f "${dir}/lib64/libcrypto.a" ]; }
}

# Build one target.
# Args: <target-config> <install-prefix> [extra Configure args...]
build_one() {
    local target="$1"
    local out="$2"
    shift 2
    local extra_args=("$@")

    if target_ready "$out"; then
        echo "OpenSSL for $target already built at $out; skipping."
        return
    fi

    echo "==============================================="
    echo "Building OpenSSL for $target"
    echo "==============================================="

    pushd "$SRC" >/dev/null

    make clean 2>/dev/null || true

    ./Configure "$target" \
        --prefix="$out" \
        --openssldir="$out/ssl" \
        no-shared no-tests no-docs no-apps no-legacy no-zlib \
        "${extra_args[@]}"

    make -j"$(nproc)"
    make install_sw
    make clean

    popd >/dev/null

    echo "OpenSSL for $target installed at $out"
}

# ---------- Build all four ----------
build_one "linux-x86_64"       "$DEPS/openssl-linux-x86_64"
build_one "linux-x86"          "$DEPS/openssl-linux-i686"   -m32

echo
echo "Done. Both Linux OpenSSL trees are ready:"
echo "  $DEPS/openssl-linux-x86_64"
echo "  $DEPS/openssl-linux-i686"