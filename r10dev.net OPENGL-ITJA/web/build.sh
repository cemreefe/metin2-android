#!/usr/bin/env bash
# Configure + build the wasm client.
#
#   ./build.sh [build-dir]            (default build-web)
#
# Needs emcmake/emmake on PATH (source ~/emsdk/emsdk_env.sh first) and the
# wasm extern libs in Extern/lib/web + Extern/include/Python2-web (build
# them with tools/wasm-deps/build.sh).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
BUILD="${1:-$ROOT/build-web}"

emcmake cmake -S "$ROOT/clientsource" -B "$BUILD" \
    -DM2_TARGET=web -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc)"

echo "-> $BUILD/metin2_web.{js,wasm} + metin2_web.html (from shell.html)"
