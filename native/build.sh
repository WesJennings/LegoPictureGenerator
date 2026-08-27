#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}"
export CXX="${CXX:-/usr/bin/g++}"
export CC="${CC:-/usr/bin/gcc}"
# If g++ is missing (e.g. macOS), fall back to the platform default.
if ! command -v "${CXX}" >/dev/null 2>&1; then
  CXX="$(command -v c++)"
fi
cmake -S "$ROOT/native" -B "$ROOT/native/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
cmake --build "$ROOT/native/build" -j"$(nproc)"
mkdir -p "$ROOT/backend/src/main/resources/native"
cp "$ROOT/native/build/liblegocore.so" "$ROOT/backend/src/main/resources/native/liblegocore.so"
"$ROOT/native/build/lego_native_tests"
