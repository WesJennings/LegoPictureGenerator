#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export CXX="${CXX:-/usr/bin/g++}"
export CC="${CC:-/usr/bin/gcc}"
if ! command -v "${CXX}" >/dev/null 2>&1; then
  CXX="$(command -v c++)"
fi
cmake -S "$ROOT/native" -B "$ROOT/native/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
cmake --build "$ROOT/native/build" -j"$JOBS"
"$ROOT/native/build/lego_native_tests"
"$ROOT/native/build/lego_host_tests"
